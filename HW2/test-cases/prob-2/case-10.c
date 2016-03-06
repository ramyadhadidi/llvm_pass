

int main()
{
	int x=3;
	int i,j,n;
	int sum=0;
	int y;

	for(i=0;i<n;i++)
	{
		for(j=0;j<n;j++)
		{
			sum +=x; //is x constant here
		}
	}
	
	y=x;

	if(n>100)
		x=4;	
	
	for(i=0;i<n;i++)
	{
		for(j=0;j<n;j++)
		{
			sum +=x; //is x constant here
			sum +=y; //is y constant here
		}
	}

	
	
}

