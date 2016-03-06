
int main()
{
	int x=3;
	int n;
	int rval;

	while(n>100)
	{
		rval=x; //is x un-initialized here?
		x=n;
		n--;
		x=2;
	}

	return rval;
}
