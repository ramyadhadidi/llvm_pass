

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
			sum +=x; //is x un-initialized here
		}
	}
	
	y=x;

	if(n>100)
		x=n;	
	
	for(i=0;i<n;i++)
	{
		for(j=0;j<n;j++)
		{
			sum +=x; //is x un-initialized here
			sum +=y; //is y un-initialized here
		}
	}

	
	
}

