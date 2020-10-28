// This is a sample class, serving as a minimal example
// of an UnrealScript class within a new package.
class SampleClass extends Actor;

// Declare some variables.
var int i;

// Called when gameplay begins.
function BeginPlay()
{
	SetTimer(1,true);
}

// Called every 1 second, due to SetTimer call.
function Timer()
{
	i=i+1;
	BroadcastMessage("SampleClass is counting "$i);
}

defaultproperties
{
	bHidden=false
}
