# New Model

So the new model will be composed of an entirely different structure. There will be clouds of computation forming the Prequel, Body, Epilogue, and Termination of a kernel. The old model only had the Body. This new model will start in the prequel, go to the conditional, then either to body or the termination block. Whenever we pass from the body to the prequel we will perform a stack save.

When in the Termination block we will go to the epilogue the same number of times that we saved the stack, restoring it once each time. Finally we will then exit the code.

With particularly onerous kernels containing lots of recursion a new model is needed still. This would abstract the Prequel, Conditional, Body and Epilogue into a new construct yet to be named. When we exit we don't go to Termination but instead to the next construct. (if there is a return we still go to Termination). The termination block will then go to the appropriate Epilogue and continue through the structures rather than returning directly to the Termination. Do note that if there was only one structure we would always go to the Termination anyway.

|Label|Loop|Recursion|Name|Valid Destinations|
|-|-|-|-|-|
|O|✔|✔|Init|A|
|A|❌|✔|Prequel|B|
|B|✔|✔|Conditional|C, E|
|C|✔|✔|Body|A(Stack Store)|
|D|❌|✔|Epilogue|E|
|E|❌|✔|Termination|D(Stack Restore), F|
|F|✔|✔|Exit||