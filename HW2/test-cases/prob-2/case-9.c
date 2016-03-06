

int main()
{
	int x=3;
	int i,j,n;
	int sum=0;

	for(i=0;i<n;i++)
	{
		for(j=0;j<n;j++)
		{
			sum +=x; //is x constant here
		}
	}
	
	if(n>100)
		x=4;	
	
	for(i=0;i<n;i++)
	{
		for(j=0;j<n;j++)
		{
			sum +=x; //is x constant here
		}
	}

	
	
}

