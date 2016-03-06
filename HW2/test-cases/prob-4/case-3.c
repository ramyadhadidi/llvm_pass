

int main()
{
	int x=3;
	int n;
	int rval;
	
	if(n>100)
		x=n;
	else
		x=3;

	rval=x; // is x un-initialized here?
	return rval;	
}

