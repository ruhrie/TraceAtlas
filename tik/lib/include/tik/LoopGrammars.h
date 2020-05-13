#pragma once
namespace TraceAtlas::tik
{
    enum class LoopGrammar : int
    {
        None,
        Linear,   //straight code
        Fixed,    //static range
        Dynamic,  //bound to a particular limit, but no bounds on the limit
        Internal, //controlled internally
        External  //controlled externally
    };
}
