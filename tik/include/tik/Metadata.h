#pragma once
enum class TikMetadata : int
{
    KernelFunction,
    MemoryWrite,
    MemoryRead
};

enum class TikSynthetic : int
{
    None,
    Store,
    Load,
    Cast
};