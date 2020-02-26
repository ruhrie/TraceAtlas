#pragma once
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