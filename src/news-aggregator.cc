/**
 * File: news-aggregator.cc
 * --------------------------------
 * Presents the implementation of the NewsAggregator class.
 */

#include "news-aggregator.h"
#include <string>
#include <iostream>
#include <iomanip>
#include <memory>
#include <thread>
#include <iostream>
#include <algorithm>
#include <thread>
#include <utility>

#include <getopt.h>
#include <libxml/parser.h>
#include <libxml/catalog.h>
#include "rss-feed.h"
#include "rss-feed-list.h"
#include "html-document.h"
#include "html-document-exception.h"
#include "rss-feed-exception.h"
#include "rss-feed-list-exception.h"
#include "utils.h"
#include "ostreamlock.h"
#include "string-utils.h"
using namespace std;

/**
 * Factory Method: createNewsAggregator
 * ------------------------------------
 * Factory method that spends most of its energy parsing the argument vector
 * to decide what RSS feed list to process and whether to print lots of
 * of logging information as it does so.
 */
static const string kDefaultRSSFeedListURL = "small-feed.xml";
NewsAggregator *NewsAggregator::createNewsAggregator(int argc, char *argv[]) {
  struct option options[] = {
    {"verbose", no_argument, NULL, 'v'},
    {"quiet", no_argument, NULL, 'q'},
    {"url", required_argument, NULL, 'u'},
    {NULL, 0, NULL, 0},
  };
  
  string rssFeedListURI = kDefaultRSSFeedListURL;
  bool verbose = true;
  while (true) {
    int ch = getopt_long(argc, argv, "vqu:", options, NULL);
    if (ch == -1) break;
    switch (ch) {
    case 'v':
      verbose = true;
      break;
    case 'q':
      verbose = false;
      break;
    case 'u':
      rssFeedListURI = optarg;
      break;
    default:
      NewsAggregatorLog::printUsage("Unrecognized flag.", argv[0]);
    }
  }
  
  argc -= optind;
  if (argc > 0) NewsAggregatorLog::printUsage("Too many arguments.", argv[0]);
  return new NewsAggregator(rssFeedListURI, verbose);
}

/**
 * Method: buildIndex
 * ------------------
 * Initalizex the XML parser, processes all feeds, and then
 * cleans up the parser.  The lion's share of the work is passed
 * on to processAllFeeds, which you will need to implement.
 */
void NewsAggregator::buildIndex() {
  if (built) return;
  built = true; // optimistically assume it'll all work out
  xmlInitParser();
  xmlInitializeCatalog();
  processAllFeeds();
  xmlCatalogCleanup();
  xmlCleanupParser();
}

/**
 * Method: queryIndex
 * ------------------
 * Interacts with the user via a custom command line, allowing
 * the user to surface all of the news articles that contains a particular
 * search term.
 */
void NewsAggregator::queryIndex() const {
  static const size_t kMaxMatchesToShow = 15;
  while (true) {
    cout << "Enter a search term [or just hit <enter> to quit]: ";
    string response;
    getline(cin, response);
    response = trim(response);
    if (response.empty()) break;
    const vector<pair<Article, int> >& matches = index.getMatchingArticles(response);
    if (matches.empty()) {
      cout << "Ah, we didn't find the term \"" << response << "\". Try again." << endl;
    } else {
      cout << "That term appears in " << matches.size() << " article"
           << (matches.size() == 1 ? "" : "s") << ".  ";
      if (matches.size() > kMaxMatchesToShow)
        cout << "Here are the top " << kMaxMatchesToShow << " of them:" << endl;
      else if (matches.size() > 1)
        cout << "Here they are:" << endl;
      else
        cout << "Here it is:" << endl;
      size_t count = 0;
      for (const pair<Article, int>& match: matches) {
        if (count == kMaxMatchesToShow) break;
        count++;
        string title = match.first.title;
        if (shouldTruncate(title)) title = truncate(title);
        string url = match.first.url;
        if (shouldTruncate(url)) url = truncate(url);
        string times = match.second == 1 ? "time" : "times";
        cout << "  " << setw(2) << setfill(' ') << count << ".) "
             << "\"" << title << "\" [appears " << match.second << " " << times << "]." << endl;
        cout << "       \"" << url << "\"" << endl;
      }
    }
  }
}

void NewsAggregator::updateRawMap(vector<string> tokens, pair<server, title> key, const Article& article) {
    mapLock.lock(); // Only one thread should be modifying the map at a time
    if (articleMap.find(key) == articleMap.end()) {
	// If we haven't seen this server/title pair before, add it to the raw map
        articleMap[key] = pair<Article, vector<string>>(article, tokens);
    } else {
	// Otherwise, taken the set intersection of the sorted token vectors of the current article
	// and what we already have in the map for this article
        pair<Article, vector<string>> curr = articleMap[key];
        vector<string> currTokens = curr.second;
        sort(currTokens.begin(), currTokens.end());
        sort(tokens.begin(), tokens.end());
        vector<string> tokens_intersect;
        set_intersection(currTokens.begin(), currTokens.end(), tokens.begin(), tokens.end(),
                         back_inserter(tokens_intersect));

	// Save the URL that comes first lexicographically
	if (article.url > curr.first.url) {
            articleMap[key] = pair<Article, vector<string>>(curr.first, tokens_intersect);
        } else {
            articleMap[key] = pair<Article, vector<string>>(article, tokens_intersect);
        }
    }
    mapLock.unlock();
}

void NewsAggregator::runArticleThread(const Article& article) {
    // Make sure this isn't a duplicate URL
    seenLock.lock();
    if (seenURLs.find(article.url) != seenURLs.end()) {
        seenLock.unlock();
        log.noteSingleArticleDownloadSkipped(article);
        return;
    }
    seenURLs.insert(article.url);
    seenLock.unlock();

    const string& articleTitle = article.title;
    const server& articleServer = getURLServer(article.url);

    HTMLDocument document(article.url);

    log.noteSingleArticleDownloadBeginning(article);
    try {
        document.parse();
    } catch (const HTMLDocumentException& hde) {
        log.noteSingleArticleDownloadFailure(article);
        return;
    }

    // Most of the legwork goes here
    updateRawMap(document.getTokens(), pair<server, title>(articleServer, articleTitle), article);
}

void NewsAggregator::runFeedThread(const pair<url, string>& f) {
    const url& feedUrl = f.first;
    // Check that we haven't seen this feed URI before, returning if we have
    seenLock.lock();
    if (seenURLs.find(feedUrl) != seenURLs.end()) {
        seenLock.unlock();
        log.noteSingleFeedDownloadSkipped(feedUrl);
        return;
    }
    seenURLs.insert(feedUrl);
    seenLock.unlock();

    RSSFeed feed(feedUrl);

    log.noteSingleFeedDownloadBeginning(feedUrl);
    try {
        feed.parse();
    } catch (const RSSFeedException& rfe) {
        log.noteSingleFeedDownloadFailure(feedUrl);
        return;
    }
    const vector<Article>& articles = feed.getArticles();

    semaphore completed(1 - articles.size());
    for (const Article& article : articles) {
        articlePool.schedule([this, &article, &completed] {
            runArticleThread(article); // Schedule a thread for this article
	    completed.signal();
        });
    }
    log.noteAllArticlesHaveBeenScheduledForFeed(feedUrl);
    completed.wait(); // wait for this feed to have downloaded all articles
}

/**
 * Private Constructor: NewsAggregator
 * -----------------------------------
 * Self-explanatory.
 */
static const size_t kNumFeedWorkers = 8;
static const size_t kNumArticleWorkers = 64;
NewsAggregator::NewsAggregator(const string& rssFeedListURI, bool verbose): 
    log(verbose), rssFeedListURI(rssFeedListURI), built(false), feedPool(kNumFeedWorkers),
    articlePool(kNumArticleWorkers), seenURLs(), seenLock(),
    articleMap(), mapLock() {}

/**
 * Private Method: processAllFeeds
 * -------------------------------
 * The provided code (commented out, but it compiles) illustrates how one can
 * programmatically drill down through an RSSFeedList to arrive at a collection
 * of RSSFeeds, each of which can be used to fetch the series of articles in that feed.
 *
 * You'll want to erase much of the code below and ultimately replace it with
 * your multithreaded aggregator.
 */
void NewsAggregator::processAllFeeds() {
    RSSFeedList feedList(rssFeedListURI);
    try {
        feedList.parse();
    } catch (const RSSFeedListException& rfle) {
        log.noteFullRSSFeedListDownloadFailureAndExit(rssFeedListURI);
    }
    log.noteFullRSSFeedListDownloadEnd();

    const map<url, string>& feeds = feedList.getFeeds();

    for (const pair<url, string>& f : feeds) {
        feedPool.schedule([this, f] {
            runFeedThread(f); // Schedule this feed
        });
    }
    log.noteAllFeedsHaveBeenScheduledForFeedList(rssFeedListURI);

    // Wait for all the threads to be idle, then add our final article objects to the real index.
    feedPool.wait();
    articlePool.wait();
    log.noteAllRSSFeedsDownloadEnd();
    for (auto& pair : articleMap) {
	index.add(pair.second.first, pair.second.second);
    }
}
