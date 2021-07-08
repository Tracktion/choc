//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Clean Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2021 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_SAMPLE_BUFFER_UTILS_HEADER_INCLUDED
#define CHOC_SAMPLE_BUFFER_UTILS_HEADER_INCLUDED

#include "choc_SampleBuffers.h"
#include "../containers/choc_Value.h"

//==============================================================================
namespace choc::buffer
{

/// Helper class which holds an InterleavedBuffer which it re-uses as intermediate
/// storage when creating a temporary interleaved copy of a ChannelArrayView
template <typename SampleType>
struct InterleavingScratchBuffer
{
    InterleavedBuffer<SampleType> buffer;

    template <typename SourceBufferType>
    InterleavedView<SampleType> interleave (const SourceBufferType& source)
    {
        InterleavedView<SampleType> dest;
        auto sourceSize = source.getSize();

        if (buffer.getNumChannels() < sourceSize.numChannels || buffer.getNumFrames() < sourceSize.numFrames)
        {
            buffer.resize (source.getSize());
            dest = buffer.getView();
        }
        else
        {
            dest = buffer.getSection ({ 0, sourceSize.numChannels },
                                      { 0, sourceSize.numFrames });
        }

        copy (dest, source);
        return dest;
    }
};

/// Helper class which holds a ChannelArrayBuffer which it re-uses as intermediate
/// storage when creating a temporary channel-based copy of an InterleavedView
template <typename SampleType>
struct DeinterleavingScratchBuffer
{
    ChannelArrayBuffer<SampleType> buffer;

    template <typename SourceBufferType>
    ChannelArrayView<SampleType> deinterleave (const SourceBufferType& source)
    {
        ChannelArrayView<SampleType> dest;
        auto sourceSize = source.getSize();

        if (buffer.getNumChannels() < sourceSize.numChannels || buffer.getNumFrames() < sourceSize.numFrames)
        {
            buffer.resize (source.getSize());
            dest = buffer.getView();
        }
        else
        {
            dest = buffer.getSection ({ 0, sourceSize.numChannels },
                                      { 0, sourceSize.numFrames });
        }

        copy (dest, source);
        return dest;
    }
};

}

#endif