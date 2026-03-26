
void matmul(float a[16][16], float b[16][16], float c[16][16]) 
{
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            for (int k = 0; k < 16; k++)
            {
                c[i][j] += a[i][k] * b[k][j];
                c[j][k] += a[i][k] * b[k][j];
            }   
        }    
    }
}