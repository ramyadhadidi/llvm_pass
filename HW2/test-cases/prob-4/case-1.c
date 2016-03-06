

int main()
{
	int x;
	int n;
	int rval;
	
	if(n>100)
		x=2;
	else
		x=4;

	rval=x; // is x un-initialized here?
	return rval;	
}

