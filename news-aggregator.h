/**
 * File: news-aggregator.h
 * -----------------------
 * Defines the NewsAggregator class.  While it is smart enough to limit the number of threads that
 * can exist at any one time, it does not try to conserve threads by pooling or reusing them.
 * Assignment 6 will revisit this same exact program, where you'll reimplement the NewsAggregator
 * class to reuse threads instead of spawning new ones for every download.
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <memory>

#include "log.h"
#include "rss-index.h"
#include "html-document.h"
#include "article.h"
#include "thread-pool-release.h"
#include "thread-pool.h"

namespace tp = release;
using tp::ThreadPool;

class NewsAggregator {
  
 public:
/**
 * Factory Method: createNewsAggregator
 * ------------------------------------
 * Static factory method that parses the command line
 * arguments to decide what RSS feed list should be downloaded
 * and parsed for its RSS feeds, which are themselves parsed for
 * their news articles, all in the pursuit of compiling one big, bad index.
 */
  static NewsAggregator *createNewsAggregator(int argc, char *argv[]);

/**
 * Method: buildIndex
 * ------------------
 * Pulls the embedded RSSFeedList, parses it, parses the
 * RSSFeeds, and finally parses the HTMLDocuments they
 * reference to actually build the index.
 */
  void buildIndex();

/**
 * Method: queryIndex
 * ------------------
 * Provides the read-query-print loop that allows the user to
 * query the index to list articles.
 */
  void queryIndex() const;

 private:

/**
 * Method: runFeedThread
 * ---------------------
 * Downloads all the articles from a single feed by scheduling them
 * all via the articlePool.
 */  
  void runFeedThread(const std::pair<std::string, std::string>&);

/**
 * Method: runArticleThread
 * ------------------------
 * Run by a single worker in articlePool to download a given article
 * and add it to the raw index if it hasn't yet been downloaded. Includes
 * the work of intersecting tokens with other versions of the same article.
 */
  void runArticleThread(const Article&);
  
/** Method: updateRawMap
 *  --------------------
 *  Updates the raw map index performing intersection on articles we've seen and
 *  adding articles we haven't.
 */

  void updateRawMap(std::vector<std::string>, std::pair<std::string, std::string>, const Article&);
/**
 * Private Types: url, server, title
 * ---------------------------------
 * All synonyms for strings, but useful so
 * that something like pair<string, string> can
 * instead be declared as a pair<server, title>
 * so it's clear that each string is being used
 * to store.
 */
  typedef std::string url;
  typedef std::string server;
  typedef std::string title;
  
  NewsAggregatorLog log;
  std::string rssFeedListURI;
  RSSIndex index;
  bool built = false;
  ThreadPool feedPool;
  ThreadPool articlePool;

  std::set<url> seenURLs; // URLs we've already seen
  std::mutex seenLock; // Lock around checking and modifying the set

  // Our raw index -- maps server prefixes and article titles to Articles and tokens.
  std::map<std::pair<server, title>, std::pair<Article, std::vector<std::string>>> articleMap;
  std::mutex mapLock; // Lock around modifying and checking this raw index
  
/**
 * Constructor: NewsAggregator
 * ---------------------------
 * Private constructor used exclusively by the createNewsAggregator function
 * (and no one else) to construct a NewsAggregator around the supplied URI.
 */
  NewsAggregator(const std::string& rssFeedListURI, bool verbose);

/**
 * Method: processAllFeeds
 * -----------------------
 * Downloads all of the feeds and news articles to build the index.
 * You need to implement this function.
 */
  void processAllFeeds();

/**
 * Copy Constructor, Assignment Operator
 * -------------------------------------
 * Explicitly deleted so that one can only pass NewsAggregator objects
 * around by reference.  These two deletions are often in place to
 * forbid large objects from being copied.
 */
  NewsAggregator(const NewsAggregator& original) = delete;
  NewsAggregator& operator=(const NewsAggregator& rhs) = delete;
};
