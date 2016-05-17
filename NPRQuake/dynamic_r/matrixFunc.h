void vecAdd(GLfloat a[3], GLfloat b[3], GLfloat c[3]) {
	c[0] = a[0] + b[0];
	c[1] = a[1] + b[1];
	c[2] = a[2] + b[2];
}
void vecSub(GLfloat a[3], GLfloat b[3], GLfloat c[3]) {
	c[0] = a[0] - b[0];
	c[1] = a[1] - b[1];
	c[2] = a[2] - b[2];
}
void vecScale(GLfloat a[3], GLfloat scale, GLfloat b[3]) {
	b[0] = a[0]*scale ;
	b[1] = a[1]*scale ;
	b[2] = a[2]*scale ;
}

GLfloat vecDotProduct (GLfloat a[3], GLfloat b[3])  {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void vecCross (GLfloat a[3], GLfloat b[3], GLfloat c[3])  {
	c[0] = a[1]*b[2] - a[2]*b[1];
	c[1] = a[2]*b[0] - a[0]*b[2];
	c[2] = a[0]*b[1] - a[1]*b[0]; 
}

GLfloat vecMagn(GLfloat a[3]) {return (sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]));}

void vecNormalize(GLfloat a[3]) {
	double magn = sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
	if (magn > 0) {
	a[0] /= magn;
	a[1] /= magn;
	a[2] /= magn;
	}
	else 
	;
	/*printf("zero magn\n");*/
}
void vecPrint(GLfloat a[3], char *name) {
	printf("%s = %3.5f %3.5f %3.5f\n", name, a[0], a[1], a[2]) ;
}

void matTranspose(int size, GLfloat *mat, GLfloat *tmat)
{
  int i, j;
  for (i = 0 ; i < size; i++)
    for (j = 0 ; j < size; j++)
      tmat[i*size+j] = mat[j*size + i];
}

void matIdentity(GLfloat a[16])
{
  int i, j;
  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      a[4*i + j] = 0;
  for (i = 0; i < 4; i++)
    a[4*i + i] = 1.0;
}

void mat4Mult(GLfloat a[16], GLfloat b[16], GLfloat c[16])
{
  int i, j;
  
  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      c[4*i + j] = a[4*i + 0] * b[4*0+j] +
	a[4*i + 1] * b[4*1+j] +
	a[4*i + 2] * b[4*2+j] +
	a[4*i + 3] * b[4*3+j];
}

/* mult by  transformation matrix of the form [R|t] */
void mat4vec3Mult(GLfloat mat[16], GLfloat v[3], GLfloat vv[3] )
{
	int i, j;
	int isNormalFlag = 1;

	for (i = 0 ; i < 3; i++)
		vv[i] = 0.0;
	for (i = 0 ; i < 3; i++) {
		for (j = 0 ; j < 3; j++)
			vv[i] += mat[j*4 + i]*v[j];
  	}
	if (isNormalFlag == 1) 
	  for (i = 0 ; i < 3; i++)
		vv[i] += mat[3*4 + i];

}

/* invert transformation matrix of the form [R|t] */
void mat4Invert(GLfloat mat[16], GLfloat imat[16])
{
	int i, j;
	GLfloat R[16];
	GLfloat T[3], invT[3];
	
	matIdentity(R);
	
	/* transpose the R  */
	for (i = 0 ; i < 3; i++)
	  for (j = 0 ; j < 3; j++)
	    R[i*4+j] = mat[j*4 + i];
	/* negate the T  */
	for (i = 0 ; i < 3; i++)
	  T[i] = - mat[3*4 + i];
	mat4vec3Mult(R, T, invT);
		
	for (i = 0 ; i < 4; i++) {
	  for (j = 0 ; j < 4; j++)
	    imat[4*i+j] = R[4*i+j];
	}
	
	for (i = 0 ; i < 3; i++)
	  imat[3*4+i] = invT[i];
}

void matPrint(GLfloat mat[16], char *name)
{
	int i, j;
	printf("%s = \n", name);
	for (i = 0 ; i < 4; i++) {
		for (j = 0 ; j < 4; j++)
			printf("\t%3.5f", mat[i*4 + j]);
		printf("\n");
	}

}
