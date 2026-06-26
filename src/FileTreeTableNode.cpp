// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bvba, Kessel-Lo, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "FileTreeTableNode.hpp"

#include <boost/lexical_cast.hpp>
#include <iostream>
#include <time.h>

#include <Wt/WIconPair.h>
#include <Wt/WStringUtil.h>
#include <Wt/WText.h>

namespace Wt {
namespace Wc {

FileTreeTableNode::FileTreeTableNode(const std::filesystem::path& path,
                                     const std::string &suffix) 
#if BOOST_FILESYSTEM_VERSION < 3
#ifndef WT_NO_STD_WSTRING
  : WTreeTableNode(Wt::widen(path.filename()), createIcon(path)),
#else
  : WTreeTableNode(path.filename(), createIcon(path)),
#endif
#else
  : WTreeTableNode(path.filename().string(), createIcon(path)),
#endif
    path_(path) ,suffix_(suffix)
{
  label()->setTextFormat(TextFormat::Plain);

  if (std::filesystem::exists(path)) {
    if (!std::filesystem::is_directory(path)) {
      int fsize = (int)std::filesystem::file_size(path);
      setColumnWidget(1, std::make_unique<WText>(std::to_string(fsize)));
      columnWidget(1)->setStyleClass("fsize");
    } else
      setSelectable(false);

    // Awful, ugly, icky code to convert a std::filesystem::file_time_type to a
    // struct tm.
    auto t = std::filesystem::last_write_time(path);
    auto sctp = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(t.time_since_epoch()));
    std::time_t timeT = std::chrono::system_clock::to_time_t(sctp);

    struct tm ttm;
#if WIN32
    localtime_s(&ttm, &timeT);
#else
    localtime_r(&timeT, &ttm);
#endif

    char c[100];
    strftime(c, 100, "%b %d %Y", &ttm);

    setColumnWidget(2, std::make_unique<WText>(c));
    columnWidget(2)->setStyleClass("date");
  }
}

std::unique_ptr<WIconPair> FileTreeTableNode::createIcon(const std::filesystem::path& path)
{
  if (std::filesystem::exists(path)
      && std::filesystem::is_directory(path))
    return std::make_unique<WIconPair>("icons/yellow-folder-closed.png",
			 "icons/yellow-folder-open.png", false);
  else
    return std::make_unique<WIconPair>("icons/document.png",
			 "icons/yellow-folder-open.png", false);
}

void FileTreeTableNode::populate()
{
  if (std::filesystem::is_directory(path_)) {
    std::set<std::filesystem::path> entries;

    for (auto const& dir_entry : std::filesystem::directory_iterator{path_})
      try {
          if ( (suffix_ == "") || dir_entry.path().extension() == suffix_ ) {
            entries.insert(dir_entry.path());
          }
      } catch (std::filesystem::filesystem_error& e) {
        std::cerr << e.what() << std::endl;
      }

    for (auto entry : entries) {
      try {
        addChildNode(std::make_unique<FileTreeTableNode>(entry));
      } catch (std::filesystem::filesystem_error& e) {
        std::cerr << e.what() << std::endl;
      }
    }
  }
}

bool FileTreeTableNode::expandable()
{
  if (!populated()) {
    return std::filesystem::is_directory(path_);
  } else
    return WTreeTableNode::expandable();
}
}
}
