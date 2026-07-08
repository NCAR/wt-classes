/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#ifndef WC_STREAM_VIEW_HPP_
#define WC_STREAM_VIEW_HPP_

#include <iosfwd>
#include <memory>

#include <Wt/WGlobal.h>
#include <Wt/WViewWidget.h>
#include <Wt/WText.h>

namespace Wt {

namespace Wc {

/** View of a stream.

\ingroup util
*/
class StreamView : public WViewWidget {
public:
    /** Constructor */
    StreamView(std::istream* input_stream);

    /** Constructor.
    \note Input stream must be \ref set_stream "set" before renderView() runs
    */
    StreamView();

    /** Set encoding of string to WString conversion.
    Defaults to UTF8.
    */
    void set_encoding(Wt::CharEncoding encoding) {
        encoding_ = encoding;
    }

    /** Set the text format */
    void set_format(Wt::TextFormat format) {
        format_ = format;
    }

    /** Configure word wrapping */
    void set_word_wrap(bool word_wrap) {
        word_wrap_ = word_wrap;
    }

    /** Set input stream.
    \note Input stream must be \ref set_stream "set" before renderView() runs.
    */
    void set_stream(std::istream* input_stream);

protected:
    /** Return WText with contents of resource.
    \see create_text
    */
    std::unique_ptr<WWidget> renderView() override;

    /** Return WText with contents of resource.
    Read input stream and return WText.
    If input_stream is not set, this method returns empty WText.
    */
    std::unique_ptr<WText> create_text(std::istream* input_stream) const;

private:
    std::istream* input_stream_;
    Wt::CharEncoding encoding_ : 8;
    Wt::TextFormat format_ : 8;
    bool word_wrap_ : 1;
};

}

}

#endif
