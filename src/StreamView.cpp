/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#include <sstream>
#include <fstream>
#include <memory>

#include <Wt/WText.h>
#include <Wt/WResource.h>

#include "StreamView.hpp"
#include "FileView.hpp"
#include "ResourceView.hpp"

namespace Wt {

namespace Wc {

StreamView::StreamView(std::istream* input_stream):
    WViewWidget(),
    input_stream_(input_stream),
    encoding_(Wt::CharEncoding::UTF8)
{ }

StreamView::StreamView():
    WViewWidget(),
    input_stream_(0),
    encoding_(Wt::CharEncoding::UTF8),
    format_(Wt::TextFormat::XHTML),
    word_wrap_(true)
{ }

void StreamView::set_stream(std::istream* input_stream) {
    input_stream_ = input_stream;
    update();
}

std::unique_ptr<WWidget> StreamView::renderView() {
    return create_text(input_stream_);
}

std::unique_ptr<WText> StreamView::create_text(std::istream* input_stream) const {
    std::string str;
    if (input_stream) {
        input_stream->seekg(0, std::ios::end);
        str.reserve(input_stream->tellg());
        input_stream->seekg(0, std::ios::beg);
        str.assign((std::istreambuf_iterator<char>(*input_stream)),
                   std::istreambuf_iterator<char>());
    }
    auto result = std::make_unique<WText>(WString(str, encoding_), format_);
    result->setWordWrap(word_wrap_);
    return result;
}

FileView::FileView(const std::string& filename):
    StreamView(),
    filename_(filename)
{ }

FileView::FileView():
    StreamView()
{ }

void FileView::set_filename(const std::string& filename) {
    filename_ = filename;
    update();
}

std::unique_ptr<WWidget> FileView::renderView() {
    std::fstream f(filename_.c_str());
    return create_text(filename_.empty() ? 0 : &f);
}

ResourceView::ResourceView(WResource* resource):
    StreamView(),
    resource_(resource)
{ }

ResourceView::ResourceView():
    StreamView(),
    resource_(0)
{ }

void ResourceView::set_resource(WResource* resource) {
    resource_ = resource;
    update();
}

std::unique_ptr<WWidget> ResourceView::renderView() {
    std::stringstream ss;
    if (resource_) {
        resource_->write(ss);
    }
    return create_text(&ss);
}

}

}
