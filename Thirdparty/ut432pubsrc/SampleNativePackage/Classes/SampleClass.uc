// This is a sample native class, to serve as a minimal
// example of a class shared between UnrealScript and C++.
class SampleClass
	extends Actor
	native;

// Declare some variables.
var int    i;
var string s;
var bool   MyBool;
var vector v;

// Here is a "native function", a function that is
// callable from UnrealScript but implemented in C++.
native function int SampleNativeFunction(int i,string s,vector v);

// Here is an "event", a function that is callable
// from C++ but implemented in UnrealScript.
event SampleEvent(int i)
{
	BroadcastMessage("We are in SampleEvent in UnrealScript i="$i);
}

// Called when gameplay begins.
function BeginPlay()
{
	SetTimer(1,true);
}

// Called every 1 second, due to SetTimer call.
function Timer()
{
	i=i+1;
	MyBool=!MyBool;
	BroadcastMessage("SampleClass is counting "$i);
	BroadcastMessage("Calling SampleNativeFunction from UnrealScript");
	SampleNativeFunction(i,"Testing",Location);
}

defaultproperties
{
	bHidden=false
}
