
int main()
{
	int x=3;
	int n;
	int rval;

	while(n>100)
	{
		rval=x; //is x constant here?
		x=3;
		n--;
	}

	return rval;
}
