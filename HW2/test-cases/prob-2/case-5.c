
int main()
{
	int x=3;
	int n;
	int rval;

	while(n>100)
	{
		rval=x; //is x constant here?
		x=4;
		n--;
		x=3;
	}

	return rval;
}
