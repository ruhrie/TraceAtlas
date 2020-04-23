#pragma once
namespace TraceAtlas::tik
{
    enum class TikMetadata : int
    {
        Conditional,
        Body,
        Terminus
    };

    enum class TikSynthetic : int
    {
        None,
        Store,
        Load,
        Cast
    };
} // namespace TraceAtlas::tik
