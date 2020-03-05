#pragma once
enum class MemoryGrammar : int
{
    None,
    Static, //static increment
    Dynamic, //variable increment
    Stream, //reads in a small portion at a time
    Block, //reads a swath
    Global //no discernable pattern
};