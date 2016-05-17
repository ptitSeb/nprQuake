// COMP 238 Final Project
// Adrian Ilie, adyilie@cs.unc.edu
// www.cs.unc.edu/~adyilie/comp238/Final/Final.htm
// Non-Photorealistic Rendering Techniques for a Game Engine
// Based on NPR Quake  by Alex Mohr (http://www.cs.wisc.edu/graphics/Gallery/NPRQuake/)
// Silhouettes code adapted form the code by Ramesh Raskar (http://www.cs.unc.edu/~raskar/)

#include "quakedef.h"
#include "dynamic_r.h"

#include "matrixFunc.h"
typedef GLfloat Vec3[3];

// this is a sine lookup table for doing vertex permutations for water/teleporters..
// - Alex
static float	turbsin[] =
{
	#include "dr_warp_sin.h"
};


//polygon cull modes
#define CULL_FRONT 1
#define CULL_BACK 0
//sil modes
#define SIL_NONE -1
#define SIL_FLAT 0
#define SIL_SHADED 1
#define SIL_TEXTURE 2
#define SIL_TOON 3
//body modes
#define BODY_FLAT 0
#define BODY_SHADED 1
#define BODY_TEXTURE 2
#define BODY_TOON 3
//wall modes
#define WALL_FLAT 0
#define WALL_SHADED 1
#define WALL_TEXTURE 2
#define WALL_TOON 3
#define WALL_SKETCH 4
//wall line modes
#define LINE_NONE -1
#define LINE_FLAT 0
#define LINE_TEXTURE 1
//crease modes
#define CREASE_NONE -1
#define CREASE_FLAT 0
//shadow modes
#define SHADOW_NONE -1
#define SHADOW_FLAT 0

//wall sketch stuff
#define RAND_TABLE_SIZE (1024)
#define SHADOW_HEIGHT (20)
static GLuint  texNum[32];
// this contains random floats in the range [0..1) -- it's used to generate random
// numbers without calling rand all the time.
static float RandTable[RAND_TABLE_SIZE];
// this is the 'current random number'
static unsigned int CurRand;

//object silhouettes
static cvar_t ainpr_sil_width = { "ainpr_sil_width", "3" };
static cvar_t ainpr_sil_mode = { "ainpr_sil_mode", "0" };
static cvar_t ainpr_sil_color = { "ainpr_sil_color", "0x808080" };
static cvar_t ainpr_body_mode = { "ainpr_body_mode", "3" };
//walls
static cvar_t ainpr_wall_mode = { "ainpr_wall_mode", "4" };
static cvar_t ainpr_line_width = { "ainpr_line_width", "3" };
static cvar_t ainpr_line_mode = { "ainpr_line_mode", "0" };
//underwater walls
static cvar_t ainpr_wall2_mode = { "ainpr_wall2_mode", "4" };
static cvar_t ainpr_line2_width = { "ainpr_line2_width", "3" };
static cvar_t ainpr_line2_mode = { "ainpr_line2_mode", "0" };
//creases
static cvar_t ainpr_crease_mode = { "ainpr_crease_mode", "-1" };
static cvar_t ainpr_crease_width = { "ainpr_crease_width", "1" };
static cvar_t ainpr_crease_color = { "ainpr_crease_color", "0xFFFFFF" };
static cvar_t ainpr_crease_angle = { "ainpr_crease_angle", "120" };
//creases
static cvar_t ainpr_shadow_mode = { "ainpr_shadow_mode", "0" };
static cvar_t ainpr_shadow_height = { "ainpr_shadow_height", "10" };
//toon shading
static cvar_t ainpr_shade_hi = { "ainpr_shade_hi", "0.6" };
static cvar_t ainpr_shade_lo = { "ainpr_shade_lo", "0.2" };
//colors in toon shading
#define NUM_COLORS 62
typedef struct color_s
{
	char	name[32];
	float	value;
	float	shade_hi, shade_lo;
} color_t;
static color_t colors[NUM_COLORS];
//character toon shading stuff
#define SHADE_WIDTH 64
static GLuint ShadeTextures[NUM_COLORS];

//other
static cvar_t ainpr_alpha = { "ainpr_alpha", "0.25" };
static cvar_t ainpr_water = { "ainpr_water", "6" };
static cvar_t ainpr_walls = { "ainpr_walls", "6" };
static cvar_t ainpr_tex_no = { "ainpr_tex_no", "31" };
//global vars
static float Ccolor;

static void (*dr_ConPrintf)( char * fmt, ... ) = NULL;
static void (*dr_VectorMA)( vec3_t, float, vec3_t, vec3_t ) = NULL;
static void (*dr_GL_Bind)( int ) = NULL;
static void (*dr_GL_DisableMultitexture)() = NULL;
static void (*dr_GL_EnableMultitexture)() = NULL;
static void (*dr_AngleVectors)( vec3_t, vec3_t, vec3_t, vec3_t ) = NULL;
static texture_t * (*dr_R_TextureAnimation)( texture_t * ) = NULL;
static void (*dr_VectorScale)( vec3_t, vec_t, vec3_t ) = NULL;

static void (*dr_RegisterVar)( cvar_t * var );
static void (*dr_UnRegisterVar)( cvar_t * var );

//copied over from common.c to parse hex strings
float Q_atof (char *str)
{
	double			val;
	int             sign;
	int             c;
	int             decimal, total;
	
	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
		sign = 1;
		
	val = 0;

//
// check for hex
//
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') )
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val*16) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val*16) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val*16) + c - 'A' + 10;
			else
				return val*sign;
		}
	}
	
//
// check for character
//
	if (str[0] == '\'')
	{
		return sign * str[1];
	}
	
//
// assume decimal
//
	decimal = -1;
	total = 0;
	while (1)
	{
		c = *str++;
		if (c == '.')
		{
			decimal = total;
			continue;
		}
		if (c <'0' || c > '9')
			break;
		val = val*10 + c - '0';
		total++;
	}

	if (decimal == -1)
		return val*sign;
	while (total > decimal)
	{
		val /= 10;
		total--;
	}
	
	return val*sign;
}

// set GL color from a hex float
void setColor(float color)
{
	float r,g,b;
	int c;
	c = (int)color;
	b = (c % 256)/256.0;
	c /= 256;
	g = (c % 256)/256.0;
	r = (c / 256)/256.0;
	glColor3f(r,g,b);
}

// set GL color from a hex float multiplied by a lighting coefficient
void setColor2(float color, float l)
{
	float r,g,b;
	int c;
	c = (int)color;
	b = (c % 256)/256.0*l;
	c /= 256;
	g = (c % 256)/256.0*l;
	r = (c / 256)/256.0*l;
	glColor3f(r,g,b);
}

//set a color vector from a hex float
void texColor(float color, float *c)
{
	int cl;
	cl = (int)color;
	c[2] = (cl % 256)/256.0;
	cl /= 256;
	c[1] = (cl % 256)/256.0;
	c[0] = (cl / 256)/256.0;
}

//get the index for a named object (toon rendering)
int getColorIndex(char *name)
{
	int c=0, index=-1;
	do
	{
		if(strstr(name,colors[c].name))
			index = c;
		c++;
	}
	while((c<NUM_COLORS)&&(index==-1));

	if(index==-1) index = NUM_COLORS-1;

	return index;
}

//get the index for a named object (toon rendering)
float getColorValue(char *name)
{
	return colors[getColorIndex(name)].value;
}

// make the 1D texture for toon rendering (once for each object)
void setTexture(char *name)
{
	int i,l;
	int modified = 0;
	float c3[3];
	float Shade[SHADE_WIDTH*3];
	int index = getColorIndex(name);
	float color = colors[index].value;

	texColor(color,c3);
	if((ainpr_shade_lo.value != colors[index].shade_lo)||
		(ainpr_shade_hi.value != colors[index].shade_hi))
	{
		l = floor(SHADE_WIDTH*ainpr_shade_lo.value)*3;
		for(i=0;i<l;i++)
			Shade[i]=0;
		l = floor(SHADE_WIDTH*ainpr_shade_hi.value)*3;
		for(;i<l;i+=3)
		{
			Shade[i]=c3[0];
			Shade[i+1]=c3[1];
			Shade[i+2]=c3[2];
		}
		for(;i<(SHADE_WIDTH)*3;i++)
			Shade[i]=1;
		modified = 1;
	}
	glBindTexture(GL_TEXTURE_1D, ShadeTextures[index]);
	if(modified==1)
	{
		// Do not allow bilinear filtering
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP);

		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, SHADE_WIDTH, 0, GL_RGB , GL_FLOAT, Shade);

	    //dr_ConPrintf( "New texture for %s\n",colors[index].name);
	}
	colors[index].shade_lo = ainpr_shade_lo.value;
	colors[index].shade_hi = ainpr_shade_hi.value;
	Ccolor = color;
}

// load wall texture (from sketch NPR Quake)
unsigned int loadRawFile(char* texFileName) {

	unsigned int dimX, dimY, numChannels;

	unsigned int temp[4];

	char x[4], y[4], numChan[2];

	unsigned int newTexID = 0;

	GLubyte* texBuffer = NULL;

	FILE* texFile = NULL;

	x[3] = y[3] = numChan[1] = 0;

	temp[0] = 0;
	temp[0] = 255;
	temp[0] = 255;
	temp[0] = 0;


	strncpy(x, &texFileName[strlen(texFileName) - 11], 3);
	strncpy(y, &texFileName[strlen(texFileName) - 7], 3);
	strncpy(numChan, &texFileName[strlen(texFileName) - 13], 1);

	dimX = atoi(x);
	dimY = atoi(y);
	numChannels = atoi(numChan);

	// number of channels is 3 if number is screwed up (no number present at beginning of file name)
	if(numChannels != 4 && numChannels != 3)
		numChannels = 3;


	// open file
	texFile = fopen( texFileName, "rb" );

	// if file not valid
	if (texFile == NULL) {
		return 0;
	}

	// allocate space for the temp texture buffer
	texBuffer = malloc(sizeof(GLubyte) * dimX * dimY * numChannels);

	// read texture data from file
	fread(texBuffer, sizeof(GLubyte), dimX * dimY * numChannels, texFile);
	fclose(texFile);

	glPixelStorei(GL_UNPACK_ALIGNMENT , 1);

	// get first available texture handle
	glGenTextures( 1, &newTexID);
	dr_GL_Bind(newTexID);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);

	
	if(numChannels == 3)
	{
//		gluBuild2DMipmaps(	GL_TEXTURE_2D, GL_RGB8, dimX, dimY, GL_RGB, GL_UNSIGNED_BYTE, 
//							texBuffer );
		glTexImage2D(   GL_TEXTURE_2D , 0 , GL_RGB8, dimX , dimY , 0 , 
						GL_RGB , GL_UNSIGNED_BYTE , texBuffer);
	}
	else if(numChannels == 4)
	{
//		gluBuild2DMipmaps( GL_TEXTURE_2D, GL_RGBA8, dimX, dimY, GL_RGBA, GL_UNSIGNED_BYTE, texBuffer );
		glTexImage2D(   GL_TEXTURE_2D , 0 , 4, dimX , dimY , 0 , 
						GL_RGBA , GL_UNSIGNED_BYTE , texBuffer);
	}

	// OpenGL has copied texture data; it is now safe to delete 
	// the temporary buffer.
	free(texBuffer);


	return newTexID;
}

//load colors for objects (colors.txt)
void loadColors(char *fileName)
{
	int c = 0;
	char val[32];
	FILE* texFile = fopen( fileName, "rb" );

	glGenTextures(NUM_COLORS,ShadeTextures);

	for(c=0; c<NUM_COLORS; c++)
	{
		fscanf(texFile,"%s %s",&colors[c].name,&val);
		colors[c].value = Q_atof(val);
		//dr_ConPrintf( "Color for %s\n",colors[c].name);

		colors[c].shade_hi = -1;
		colors[c].shade_lo = -1;
		Ccolor = -1;
		setTexture(colors[c].name);
	}
}


/*
================
triNorm -- takes a vertex pointer to a triangle's first vertex and a float array.  
The normal to the triangle at the three sequential vertices is returned in norm.
================
*/
//copied over form sketch NPR Quake
void triNorm (float *vert1, float *vert2, float *vert3, float *norm)
{
	float	length;
	vec3_t	v1, v2;

	VectorSubtract(vert1, vert2, v1);
	VectorSubtract(vert3, vert2, v2);

	norm[0] = v1[1]*v2[2] - v1[2]*v2[1];
	norm[1] = v1[2]*v2[0] - v1[0]*v2[2];
	norm[2] = v1[0]*v2[1] - v1[1]*v2[0];

	length = sqrt(norm[0]*norm[0] + norm[1]*norm[1] + norm[2]*norm[2]);
	norm[0] /= length;
	norm[1] /= length;
	norm[2] /= length;
}


// IMMEDIATELY after that, it should call this Init function.


EXPORT void dr_Init() {

	int i;

    dr_ConPrintf( "A.I. NPR renderer starting up...\n" );

    dr_ConPrintf( "registering my vars\n" );

    dr_RegisterVar( &ainpr_sil_width );
    dr_RegisterVar( &ainpr_sil_mode );
    dr_RegisterVar( &ainpr_sil_color );
    dr_RegisterVar( &ainpr_body_mode );
    dr_RegisterVar( &ainpr_wall_mode );
    dr_RegisterVar( &ainpr_line_width );
    dr_RegisterVar( &ainpr_line_mode );
    dr_RegisterVar( &ainpr_wall2_mode );
    dr_RegisterVar( &ainpr_line2_width );
    dr_RegisterVar( &ainpr_line2_mode );
    dr_RegisterVar( &ainpr_crease_mode );
    dr_RegisterVar( &ainpr_crease_width );
    dr_RegisterVar( &ainpr_crease_color );
    dr_RegisterVar( &ainpr_crease_angle );
    dr_RegisterVar( &ainpr_shade_hi );
    dr_RegisterVar( &ainpr_shade_lo );
    dr_RegisterVar( &ainpr_alpha );
    dr_RegisterVar( &ainpr_walls );
    dr_RegisterVar( &ainpr_water );
    dr_RegisterVar( &ainpr_tex_no );
    dr_RegisterVar( &ainpr_shadow_mode );
    dr_RegisterVar( &ainpr_shadow_height );

    // seed the generator with the time.
    srand( 1000 );
    for( i = 0; i < RAND_TABLE_SIZE; i++ ) {
        RandTable[i] = (float)rand() / (float)RAND_MAX;
    }

    // current random number is the first one;
    CurRand = 0;


	texNum[0] = loadRawFile("textures/new/tex1_3_128_128.raw");
	texNum[1] = loadRawFile("textures/new/tex2_3_128_128.raw");
	texNum[2] = loadRawFile("textures/new/tex3_3_128_128.raw");
	texNum[3] = loadRawFile("textures/new/tex4_3_128_128.raw");
	texNum[4] = loadRawFile("textures/new/tex5_3_128_128.raw");
	texNum[5] = loadRawFile("textures/new/tex6_3_128_128.raw");
	texNum[6] = loadRawFile("textures/new/tex7_3_128_128.raw");
	texNum[7] = loadRawFile("textures/new/tex8_3_128_128.raw");
	texNum[8] = loadRawFile("textures/new/tex9_3_128_128.raw");
	texNum[9] = loadRawFile("textures/new/tex10_3_128_128.raw");
	texNum[10] = loadRawFile("textures/new/tex11_3_128_128.raw");
	texNum[11] = loadRawFile("textures/new/tex12_3_128_128.raw");
	texNum[12] = loadRawFile("textures/new/tex13_3_128_128.raw");
	texNum[13] = loadRawFile("textures/new/tex14_3_128_128.raw");
	texNum[14] = loadRawFile("textures/new/tex15_3_128_128.raw");
	texNum[15] = loadRawFile("textures/new/tex16_3_128_128.raw");
	texNum[16] = loadRawFile("textures/new/tex17_3_128_128.raw");
	texNum[17] = loadRawFile("textures/new/tex18_3_128_128.raw");
	texNum[18] = loadRawFile("textures/new/tex19_3_128_128.raw");
	texNum[19] = loadRawFile("textures/new/tex20_3_128_128.raw");
	texNum[20] = loadRawFile("textures/old/tex1_3_128_128.raw");
	texNum[21] = loadRawFile("textures/old/tex2_3_128_128.raw");
	texNum[22] = loadRawFile("textures/old/tex3_3_128_128.raw");
	texNum[23] = loadRawFile("textures/old/tex4_3_128_128.raw");
	texNum[24] = loadRawFile("textures/old/tex5_3_128_128.raw");
	texNum[25] = loadRawFile("textures/old/tex6_3_128_128.raw");
	texNum[26] = loadRawFile("textures/old/tex7_3_128_128.raw");
	texNum[27] = loadRawFile("textures/old/tex8_3_128_128.raw");
	texNum[28] = loadRawFile("textures/old/tex9_3_128_128.raw");
	texNum[29] = loadRawFile("textures/new/tex21_3_128_128.raw");
	texNum[30] = loadRawFile("textures/new/tex22_3_128_128.raw");
	texNum[31] = loadRawFile("textures/new/tex23_3_128_128.raw");

	loadColors("colors.txt");
}



// BEFORE removing the renderer, it should call this Shutdown function.


EXPORT void dr_Shutdown() {

    dr_ConPrintf( "A.I. NPR renderer shutting down...\n" );

    dr_ConPrintf( "unregistering my vars\n" );

    dr_UnRegisterVar( &ainpr_sil_width );
    dr_UnRegisterVar( &ainpr_sil_mode );
    dr_UnRegisterVar( &ainpr_sil_color );
    dr_UnRegisterVar( &ainpr_body_mode );
    dr_UnRegisterVar( &ainpr_wall_mode );
    dr_UnRegisterVar( &ainpr_line_width );
    dr_UnRegisterVar( &ainpr_line_mode );
    dr_UnRegisterVar( &ainpr_wall2_mode );
    dr_UnRegisterVar( &ainpr_line2_width );
    dr_UnRegisterVar( &ainpr_line2_mode );
    dr_UnRegisterVar( &ainpr_crease_mode );
    dr_UnRegisterVar( &ainpr_crease_width );
    dr_UnRegisterVar( &ainpr_crease_color );
    dr_UnRegisterVar( &ainpr_crease_angle );
    dr_UnRegisterVar( &ainpr_shade_hi );
    dr_UnRegisterVar( &ainpr_shade_lo );
    dr_UnRegisterVar( &ainpr_alpha );
    dr_UnRegisterVar( &ainpr_walls );
    dr_UnRegisterVar( &ainpr_water );
    dr_UnRegisterVar( &ainpr_tex_no );
    dr_UnRegisterVar( &ainpr_shadow_mode );
    dr_UnRegisterVar( &ainpr_shadow_height );
}

// random numbers for the sketch lines on the walls and water (copied over from sketch NPR Quake)
float randFloat (float min, float max)
{   
    // I know this seems backwards, but it works and avoids an extra variable.
    if( CurRand >= RAND_TABLE_SIZE ) {
        CurRand = 0;
    }
	return( RandTable[CurRand++] * (max - min) + min );
}



// The only non-export function.


/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity, double cltime)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		dr_ConPrintf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cltime + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are >= 1, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}





/********************************************************************************/




EXPORT void dr_Set_Cvar_RegisterVariable( void (*f)( cvar_t * ) ) {

    dr_RegisterVar = f;

}



EXPORT void dr_Set_Cvar_UnRegisterVariable( void (*f)( cvar_t * ) ) {

    dr_UnRegisterVar = f;

}



EXPORT void dr_Set_ConPrintf( void (*f)( char * fmt, ... ) ) {

    dr_ConPrintf = f;

}



EXPORT void dr_Set_VectorMA( void (*f)( vec3_t, float, vec3_t, vec3_t ) ) {

    dr_VectorMA = f;

}



EXPORT void dr_Set_GL_Bind( void (*f)( int ) ) {

    dr_GL_Bind = f;

}



EXPORT void dr_Set_GL_DisableMultitexture( void (*f)() ) {

    dr_GL_DisableMultitexture = f;

}



EXPORT void dr_Set_GL_EnableMultitexture( void (*f)() ) {

    dr_GL_EnableMultitexture = f;

}



EXPORT void dr_Set_AngleVectors( void (*f)( vec3_t, vec3_t, vec3_t, vec3_t ) ) {

    dr_AngleVectors = f;

}



EXPORT void dr_Set_R_TextureAnimation( texture_t * (*f)( texture_t * ) ) {

    dr_R_TextureAnimation = f;

}



EXPORT void dr_Set_VectorScale( void (*f)( vec3_t, vec_t, vec3_t ) ) {

    dr_VectorScale = f;

}




/****************************************/

static GLenum oldtarget = TEXTURE0_SGIS;

EXPORT void GL_SelectTexture (GLenum target, qboolean gl_mtexable, 
                       lpSelTexFUNC qglSelectTextureSGIS, int currenttexture,
                       int * cnttextures) 
{
	if (!gl_mtexable)
		return;
	qglSelectTextureSGIS(target);
	if (target == oldtarget) 
		return;
	cnttextures[oldtarget-TEXTURE0_SGIS] = currenttexture;
	currenttexture = cnttextures[target-TEXTURE0_SGIS];
	oldtarget = target;
}




/*
=================
R_DrawSpriteModel

=================
*/
EXPORT void R_DrawSpriteModel (entity_t *e, entity_t *currententity,
                        double cltime, vec3_t vup, vec3_t vright)
{
	vec3_t	point;
	mspriteframe_t	*frame;
	float		*up, *right;
	vec3_t		v_forward, v_right, v_up;
	msprite_t		*psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache

	frame = R_GetSpriteFrame( e, cltime );
	psprite = currententity->model->cache.data;

	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
		dr_AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
	{	// normal sprite
		up = vup;
		right = vright;
	}

	glColor3f (1,1,1);

	dr_GL_DisableMultitexture();

    dr_GL_Bind(frame->gl_texturenum);

	glEnable (GL_ALPHA_TEST);
	glBegin (GL_QUADS);

	glTexCoord2f (0, 1);
	dr_VectorMA (e->origin, frame->down, up, point);
	dr_VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (0, 0);
	dr_VectorMA (e->origin, frame->up, up, point);
	dr_VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 0);
	dr_VectorMA (e->origin, frame->up, up, point);
	dr_VectorMA (point, frame->right, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 1);
	dr_VectorMA (e->origin, frame->down, up, point);
	dr_VectorMA (point, frame->right, right, point);
	glVertex3fv (point);
	
	glEnd ();

	glDisable (GL_ALPHA_TEST);
}

//render one crease from the vertices of a triangle and the eye position
//adapted from Ramesh Raskar's paper (http://www.cs.unc.edu/~raskar/HWWS/)
void drawCrease (float *vert1, float *vert2, float *vert3, float *eyeT )
{
	int j;

	GLfloat eyeVec[3];

	GLfloat vert01[3], vert10[3];
	Vec3 Edge[3], edgeN[3], Ncos, edgeNsin[3], edgeShift;
	float norm[3];
	float *verts [3];
	GLfloat cosThr, sinThr, cosAngle;
	float center[3];

	vecAdd(vert1, vert2, center);
	vecAdd(center, vert3, center);
	for (j=0; j<3; j++) center[j] /= 3;//the center of the triangle

	glDisable(GL_BLEND);

	verts[0]=vert1;
	verts[1]=vert2;
	verts[2]=vert3;

	triNorm(vert1, vert2, vert3, norm);

	//draw the creases
	
	cosThr = -1*ainpr_crease_width.value*cos(ainpr_crease_angle.value/180*3.14); // negative normal
	sinThr = ainpr_crease_width.value*sin(ainpr_crease_angle.value/180*3.14);


	vecSub( eyeT, center, eyeVec);
	//eyeDist = vecMagn(eyeVec);
	vecNormalize(eyeVec);
	cosAngle = vecDotProduct(eyeVec, norm);

//	if (cosAngle < 0) // remove backfacing
	{
		glBegin(GL_QUADS);

		for (j = 0; j < 3; j++)
		{
			vecSub(verts[(j+1)%3], verts[j], Edge[j]);
			vecNormalize(Edge[j]);
			vecCross(Edge[j], norm, edgeN[j]);
			vecNormalize(edgeN[j]);
		}

		/* push all three edges out along the edge-normals */
		for (j = 0; j < 3; j++)
		{
			vecScale(norm,cosThr,Ncos);
			vecScale(edgeN[j], sinThr, edgeNsin[j]);
			vecAdd(Ncos,edgeNsin[j],edgeShift);
			vecAdd(verts[j], edgeShift, vert01);
			vecAdd(verts[(j+1)%3], edgeShift, vert10);
			glVertex3fv(verts[j]);
			glVertex3fv(vert01);
			glVertex3fv(vert10);
			glVertex3fv(verts[(j+1)%3]);
		}
	    glEnd();
	}
	glEnable(GL_BLEND);

}

//adapted form sketch NPR Quake (same bug, last triangle not drawn correctly)
void AI_DrawCreases (aliashdr_t *paliashdr, int posenum, 
                        float * shadedots, float shadelight )
{
	trivertx_t	*verts;
	
	float  vert1[3];
	float  vert2[3];
	float  vert3[3];
	
	int		*order;
	int		count;
	int		tmp = 1;
	
	trivertx_t  *fanBegin;
	
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glDepthMask( GL_TRUE );

	//glEnable( GL_POLYGON_OFFSET_FILL );
	//glPolygonOffset( 1.0, 1.0 ); 
    glPolygonMode(GL_FRONT, GL_FILL);//for some reason the front polygons are backward ;)

//	glEnable(GL_POLYGON_SMOOTH);
//	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	
	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);
	
	setColor(ainpr_crease_color.value) ;

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			fanBegin = verts;
			
			do
			{//fan
				order += 2;
				
				if(count>2){
					
					vert1[0] = (float)fanBegin->v[0];
					vert1[1] = (float)fanBegin->v[1];
					vert1[2] = (float)fanBegin->v[2];
					vert2[0] = (float)(verts+1)->v[0];
					vert2[1] = (float)(verts+1)->v[1];
					vert2[2] = (float)(verts+1)->v[2];
					vert3[0] = (float)(verts+2)->v[0];
					vert3[1] = (float)(verts+2)->v[1];
					vert3[2] = (float)(verts+2)->v[2];

					drawCrease(vert1, vert2, vert3, paliashdr->eyeposition);
				}

				verts++;
				
			} while (--count);
		} else {
			
			do
			{//strip
				order += 2;
				if(count>2){
					
					vert1[0] = (float)verts->v[0];
					vert1[1] = (float)verts->v[1];
					vert1[2] = (float)verts->v[2];
					vert2[0] = (float)(verts+1)->v[0];
					vert2[1] = (float)(verts+1)->v[1];
					vert2[2] = (float)(verts+1)->v[2];
					vert3[0] = (float)(verts+2)->v[0];
					vert3[1] = (float)(verts+2)->v[1];
					vert3[2] = (float)(verts+2)->v[2];

					drawCrease(vert1, vert2, vert3, paliashdr->eyeposition);
				}					
				verts++;
			} while (--count);
		}
		
	}
	
	//glDisable( GL_POLYGON_OFFSET_FILL );
	glDepthMask( 1 );
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}

//draw one animated object
void AI_DrawAliasFrame (aliashdr_t *paliashdr, int posenum, 
                        float * shadedots, float shadelight, int cm,
						entity_t * currententity )
{
	float 	l;
	trivertx_t	*verts;
	int		*order;
	int		count;

	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
			glBegin (GL_TRIANGLE_STRIP);

		do
		{
			if(cm==CULL_FRONT)//only light when front facing
			{
				if (ainpr_body_mode.value==BODY_SHADED)//just flat
				{
					// normals and vertexes come from the frame list
					l = shadedots[verts->lightnormalindex] * shadelight;
					setColor2 (getColorValue(currententity->model->name), l);
				}
				else if (ainpr_body_mode.value==BODY_TEXTURE)//textured, too
				{
					// normals and vertexes come from the frame list
					l = shadedots[verts->lightnormalindex] * shadelight;
					glColor3f (l, l, l);
					// texture coordinates come from the draw list
					glTexCoord2f (((float *)order)[0], ((float *)order)[1]);
				}
				else if (ainpr_body_mode.value==BODY_TOON)//toon shading
				{
					// normals and vertexes come from the frame list
					l = shadedots[verts->lightnormalindex] * shadelight;
					glTexCoord1f (l);
				}
			}
			else//back facing, so sil
			{
				if (ainpr_sil_mode.value==SIL_SHADED)//shaded
				{
					// normals and vertexes come from the frame list
					l = shadedots[verts->lightnormalindex] * shadelight;
					setColor2 (ainpr_sil_color.value, l);
				}
				else if (ainpr_sil_mode.value==SIL_TEXTURE)//textured
				{
					// normals and vertexes come from the frame list
					l = shadedots[verts->lightnormalindex] * shadelight;
					glColor3f (l, l, l);
					// texture coordinates come from the draw list
					glTexCoord2f (((float *)order)[0], ((float *)order)[1]);
				}
				else if (ainpr_sil_mode.value==SIL_TOON)//textured
				{
					// normals and vertexes come from the frame list
					l = shadedots[verts->lightnormalindex] * shadelight;
					glTexCoord1f (l);
				}
			}

			order += 2;

			glVertex3f (verts->v[0], verts->v[1], verts->v[2]);
			verts++;
		} while (--count);

		glEnd ();
	}

}

/*
=============
AI_DrawAliasShadow
//for some reason, GL_DrawAliasShadow never gets called...
=============
*/
void AI_DrawAliasShadow (aliashdr_t *paliashdr, int posenum, 
                        float * shadedots, float shadelight, 
						entity_t * currententity )
{
	trivertx_t	*verts;
	int		*order;
	int		count;

	int shadowable = 1;

	if( !strncmp( &(currententity->model->name[6]), "wiz", 3) )
		shadowable = 0;
	else if( !strncmp( &(currententity->model->name[6]), "gib", 3) )
		shadowable = 0;
	else if( !strncmp( &(currententity->model->name[6]), "ger", 3) )
		shadowable = 0;
	else if( !strncmp( &(currententity->model->name[6]), "fla", 3) )
		shadowable = 0;

	if(shadowable != 1) return;

	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_CULL_FACE);
	glColor4f(0.2f, 0.2f, 0.2f, ainpr_alpha.value) ;

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
			glBegin (GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) glTexCoord2fv ((float *)order);
			order += 2;

			glVertex3f (verts->v[0], verts->v[1], ainpr_shadow_height.value);

			verts++;
		} while (--count);

		glEnd ();
	}	
}


/*
=============
GL_DrawAliasFrame
=============
*/
EXPORT void GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum, 
                        float * shadedots, float shadelight, 
						entity_t * currententity )
{
	char *name = currententity->model->name;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

    //dr_ConPrintf( "%s\n", name );
	
	if ((ainpr_body_mode.value==BODY_TOON)||(ainpr_sil_mode.value==SIL_TOON))
	{
		setTexture(name);
		glDisable(GL_LIGHTING);
		glColor3f (1, 1, 1);
	}

	//glEnable( GL_POLYGON_OFFSET_FILL );
	//glPolygonOffset( 1.0, 0.0 ); 

	// Front
	if (ainpr_body_mode.value==BODY_FLAT)
	{
		glShadeModel(GL_FLAT);
		setColor(getColorValue(name));
	}
	else glShadeModel(GL_SMOOTH);
	if (ainpr_body_mode.value!=BODY_TEXTURE) glDisable(GL_TEXTURE_2D);
		else glEnable(GL_TEXTURE_2D);
	if (ainpr_body_mode.value!=BODY_TOON) glDisable(GL_TEXTURE_1D);
		else glEnable(GL_TEXTURE_1D);
    glCullFace(GL_FRONT);
	glEnable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);
	AI_DrawAliasFrame(paliashdr, posenum, shadedots, shadelight, CULL_FRONT, currententity);

	if (ainpr_sil_mode.value!=SIL_NONE)
	{
		// Back
		if (ainpr_sil_mode.value==SIL_FLAT)
		{
			glShadeModel(GL_FLAT);
			setColor(ainpr_sil_color.value);
		}
		else glShadeModel(GL_SMOOTH);
		if (ainpr_sil_mode.value!=SIL_TEXTURE) glDisable(GL_TEXTURE_2D);
			else glEnable(GL_TEXTURE_2D);
		if (ainpr_sil_mode.value!=SIL_TOON) glDisable(GL_TEXTURE_1D);
			else glEnable(GL_TEXTURE_1D);
	    glPolygonMode(GL_FRONT, GL_LINE);//for some reason the front polygons are backward ;)
		glDepthFunc(GL_LEQUAL);
	    glCullFace(GL_BACK);
		glDepthMask(GL_FALSE);	/* depth buffer is read only */
	    glLineWidth(ainpr_sil_width.value);
		AI_DrawAliasFrame(paliashdr, posenum, shadedots, shadelight, CULL_BACK, currententity);

	}

	if (ainpr_shadow_mode.value!=SHADOW_NONE)
		AI_DrawAliasShadow (paliashdr, posenum, shadedots, shadelight, currententity );

	
	glDepthFunc(GL_LEQUAL);

	if (ainpr_crease_mode.value!=CREASE_NONE)
		AI_DrawCreases(paliashdr, posenum, shadedots, shadelight );

	glPopAttrib();
}



/*
=============
GL_DrawAliasShadow
=============
*/
//extern	vec3_t			lightspot;

EXPORT void GL_DrawAliasShadow (aliashdr_t *paliashdr, entity_t * currententity,
                         int posenum, vec3_t shadevector,
                         vec3_t lightspot)
{
	trivertx_t	*verts;
	int		*order;
	vec3_t	point;
	float	height, lheight;
	int		count;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	height = -lheight + 1.0;
	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
			glBegin (GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) glTexCoord2fv ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;
//			height -= 0.001;
			glVertex3fv (point);

			verts++;
		} while (--count);

		glEnd ();
	}	
}






/*
============
R_PolyBlend
============
*/
EXPORT void R_PolyBlend (float * v_blend, cvar_t * gl_polyblend)
{
	if (!gl_polyblend->value)
		return;
	if (!v_blend[3])
		return;

	dr_GL_DisableMultitexture();

	glDisable (GL_ALPHA_TEST);
	glEnable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_TEXTURE_2D);

    glLoadIdentity ();

    glRotatef (-90,  1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up

	glColor4fv (v_blend);

	glBegin (GL_QUADS);

	glVertex3f (10, 100, 100);
	glVertex3f (10, -100, 100);
	glVertex3f (10, -100, -100);
	glVertex3f (10, 100, -100);
	glEnd ();

	glDisable (GL_BLEND);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_ALPHA_TEST);
}





/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
EXPORT void R_DrawSequentialPoly (msurface_t *s, int lightmap_textures,
                           qboolean * lightmap_modified, 
                           glRect_t * lightmap_rectchange, byte * lightmaps,
                           int lightmap_bytes, int solidskytexture, 
                           float * speedscale, int alphaskytexture, 
                           qboolean gl_mtexable, int currenttexture,
                           int * cnttextures, double realtime, 
                           lpMTexFUNC qglMTexCoord2fSGIS, int gl_lightmap_format,
                           lpSelTexFUNC qglSelectTextureSGIS, vec3_t r_origin )
{
	glpoly_t	*p;
	float		*v;
	int			i;
	texture_t	*t;
	vec3_t		nv;
	glRect_t	*theRect;

	//
	// normal lightmaped poly
	//

	if (! (s->flags & (SURF_DRAWSKY|SURF_DRAWTURB|SURF_UNDERWATER) ) )
	{
		//R_RenderDynamicLightmaps (s);
		if (gl_mtexable) {
			p = s->polys;

			t = dr_R_TextureAnimation (s->texinfo->texture);
			// Binds world to texture env 0
			GL_SelectTexture(TEXTURE0_SGIS, gl_mtexable, qglSelectTextureSGIS, 
                currenttexture, cnttextures );
			dr_GL_Bind (t->gl_texturenum);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			// Binds lightmap to texenv 1
			dr_GL_DisableMultitexture(); // Same as SelectTexture (TEXTURE1)
			dr_GL_Bind (lightmap_textures + s->lightmaptexturenum);
			i = s->lightmaptexturenum;
			if (lightmap_modified[i])
			{
				lightmap_modified[i] = false;
				theRect = &lightmap_rectchange[i];
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					BLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
					lightmaps+(i* BLOCK_HEIGHT + theRect->t) *BLOCK_WIDTH*lightmap_bytes);
				theRect->l = BLOCK_WIDTH;
				theRect->t = BLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
			glBegin(GL_POLYGON);
			v = p->verts[0];
			for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
			{
				qglMTexCoord2fSGIS (TEXTURE0_SGIS, v[3], v[4]);
				qglMTexCoord2fSGIS (TEXTURE1_SGIS, v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
			return;
		} else {
			p = s->polys;

			t = dr_R_TextureAnimation (s->texinfo->texture);
			dr_GL_Bind (t->gl_texturenum);
			glBegin (GL_POLYGON);
			v = p->verts[0];
			for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
			{
				glTexCoord2f (v[3], v[4]);
				glVertex3fv (v);
			}
			glEnd ();
/*
			dr_GL_Bind (lightmap_textures + s->lightmaptexturenum);
			glEnable (GL_BLEND);
			glBegin (GL_POLYGON);
			v = p->verts[0];
			for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
			{
				glTexCoord2f (v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
*/
			glDisable (GL_BLEND);
		}

		return;
	}

	//
	// subdivided water surface warp
	//

	if (s->flags & SURF_DRAWTURB)
	{
		dr_GL_DisableMultitexture();
		dr_GL_Bind (s->texinfo->texture->gl_texturenum);
		EmitWaterPolys( s, realtime );
		return;
	}

	//
	// subdivided sky warp
	//
	if (s->flags & SURF_DRAWSKY)
	{
		dr_GL_DisableMultitexture();
		dr_GL_Bind (solidskytexture);
		*speedscale = realtime*8;
		*speedscale -= (int)(*speedscale) & ~127;

		EmitSkyPolys (s, *speedscale, r_origin);

		glEnable (GL_BLEND);
		dr_GL_Bind (alphaskytexture);
		*speedscale = realtime*16;
		*speedscale -= (int)(*speedscale) & ~127;
		EmitSkyPolys (s, *speedscale, r_origin);

		glDisable (GL_BLEND);
		return;
	}

	//
	// underwater warped with lightmap
	//
	//R_RenderDynamicLightmaps (s);
	if (gl_mtexable) {
		p = s->polys;

		t = dr_R_TextureAnimation (s->texinfo->texture);
    	GL_SelectTexture(TEXTURE0_SGIS, gl_mtexable, qglSelectTextureSGIS, 
                currenttexture, cnttextures );
		dr_GL_Bind (t->gl_texturenum);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		dr_GL_DisableMultitexture();
		dr_GL_Bind (lightmap_textures + s->lightmaptexturenum);
		i = s->lightmaptexturenum;
		if (lightmap_modified[i])
		{
			lightmap_modified[i] = false;
			theRect = &lightmap_rectchange[i];
//			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
//				BLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
//				lightmaps+(i* BLOCK_HEIGHT + theRect->t) *BLOCK_WIDTH*lightmap_bytes);
			theRect->l = BLOCK_WIDTH;
			theRect->t = BLOCK_HEIGHT;
			theRect->h = 0;
			theRect->w = 0;
		}
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
		glBegin (GL_TRIANGLE_FAN);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			qglMTexCoord2fSGIS (TEXTURE0_SGIS, v[3], v[4]);
			qglMTexCoord2fSGIS (TEXTURE1_SGIS, v[5], v[6]);

			nv[0] = v[0] + 8*sin(v[1]*0.05+realtime)*sin(v[2]*0.05+realtime);
			nv[1] = v[1] + 8*sin(v[0]*0.05+realtime)*sin(v[2]*0.05+realtime);
			nv[2] = v[2];

			glVertex3fv (nv);
		}
		glEnd ();

	} else {
		p = s->polys;

		t = dr_R_TextureAnimation (s->texinfo->texture);
		dr_GL_Bind (t->gl_texturenum);
		DrawGLWaterPoly( p, realtime );

		dr_GL_Bind (lightmap_textures + s->lightmaptexturenum);
		glEnable (GL_BLEND);
		DrawGLWaterPolyLightmap( p, realtime );
		glDisable (GL_BLEND);
	}
}











/*
================
DrawGLWaterPoly

Warp the vertex coordinates
================
*/
EXPORT void DrawGLWaterPoly (glpoly_t *p, double realtime)
{
	int		i;
	float	*v;
	vec3_t	nv;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	//dr_GL_DisableMultitexture();
	if ((ainpr_line2_mode.value!=LINE_TEXTURE)&&
		(ainpr_wall2_mode.value!=WALL_TEXTURE)&&
		(ainpr_wall2_mode.value!=WALL_SKETCH))
		glDisable(GL_TEXTURE_2D);
	else
		glEnable(GL_TEXTURE_2D);
	
	if(ainpr_wall2_mode.value==WALL_SKETCH)
		dr_GL_Bind(texNum[(int)ainpr_tex_no.value]);

	glEnable( GL_POLYGON_OFFSET_FILL );
	glPolygonOffset( 1.0, 1.0 ); 

    glPolygonMode(GL_BACK, GL_FILL);
	setColor(getColorValue("wall2."));
	glBegin (GL_TRIANGLE_FAN);
	v = p->verts[0];
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		if((ainpr_wall2_mode.value==WALL_TEXTURE)||(ainpr_wall2_mode.value==WALL_SKETCH))
			glTexCoord2f (v[3], v[4]);

		nv[0] = v[0] + 8*sin(v[1]*0.05+realtime)*sin(v[2]*0.05+realtime);
		nv[1] = v[1] + 8*sin(v[0]*0.05+realtime)*sin(v[2]*0.05+realtime);
		nv[2] = v[2];

		glVertex3fv (nv);
	}
	glEnd ();

	if(ainpr_line2_mode.value!=LINE_NONE)
	{
		glPolygonMode(GL_BACK, GL_LINE);
		setColor(getColorValue("line2."));
		glLineWidth(ainpr_line2_width.value);
		glBegin (GL_TRIANGLE_FAN);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			if(ainpr_line2_mode.value==LINE_TEXTURE)
				glTexCoord2f (v[3], v[4]);

			nv[0] = v[0] + 8*sin(v[1]*0.05+realtime)*sin(v[2]*0.05+realtime);
			nv[1] = v[1] + 8*sin(v[0]*0.05+realtime)*sin(v[2]*0.05+realtime);
			nv[2] = v[2];

			glVertex3fv (nv);
		}
		glEnd ();
	}

	glPopAttrib();

}


//adapted from sketch NPR Quake (took out randomness)
EXPORT void DrawGLWaterPolyLightmap (glpoly_t *p, double realtime)
{
	int		i;
	float	*v;
	vec3_t	nv;
	int j,k;

	dr_GL_DisableMultitexture();

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc (GL_ZERO,GL_ONE_MINUS_SRC_COLOR);

	glBegin (GL_TRIANGLE_FAN);
	v = p->verts[0];
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[5], v[6]);

		nv[0] = v[0] + 8*sin(v[1]*0.05+realtime)*sin(v[2]*0.05+realtime);
		nv[1] = v[1] + 8*sin(v[0]*0.05+realtime)*sin(v[2]*0.05+realtime);
		nv[2] = v[2];
		
		glVertex3fv (nv);
	}
	glEnd ();

	if (ainpr_wall_mode.value!=WALL_SKETCH) return;

	CurRand = 0;

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND) ;
	glBlendFunc (GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	glLineWidth(ainpr_line_width.value) ;
	glColor4f(0.0f, 0.0f, 0.0f, ainpr_alpha.value) ;
	
	for (j=0; j < ainpr_walls.value; j++)
	{
		glBegin (GL_LINE_LOOP) ;
		v = p->verts[0];
		for (k=0 ; k<p->numverts ; k++, v+= VERTEXSIZE)
		{
			glVertex3f (v[0] + randFloat (-1.0f, 1.0f), v[1] + randFloat (-1.0f, 1.0f), v[2] + randFloat (-1.0f, 1.0f)) ;
		}
		glEnd ();
	}
	glLineWidth(1.0) ;
}

/*
================
DrawGLPoly
================
*/
//adapted from sketch NPR Quake (took out randomness)
EXPORT void DrawGLPoly (glpoly_t *p)
{
	int		i;
	float	*v;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glEnable( GL_POLYGON_OFFSET_FILL );
	glPolygonOffset( 1.0, 1.0 ); 

	//dr_GL_DisableMultitexture();
	if ((ainpr_line_mode.value!=LINE_TEXTURE)&&
		(ainpr_wall_mode.value!=WALL_TEXTURE)&&
		(ainpr_wall_mode.value!=WALL_SKETCH))
		glDisable(GL_TEXTURE_2D);
	else
		glEnable(GL_TEXTURE_2D);
	
	if(ainpr_wall_mode.value==WALL_SKETCH)
		dr_GL_Bind(texNum[(int)ainpr_tex_no.value]);

	if (ainpr_wall_mode.value==WALL_TOON)
	{
		glDisable(GL_LIGHTING);
		setTexture("wall.");
		glEnable(GL_TEXTURE_1D);
	}
	else glDisable(GL_TEXTURE_1D);


    //glPolygonMode(GL_BACK, GL_FILL);
	setColor(getColorValue("wall."));
	glDisable(GL_BLEND);

	glBegin (GL_POLYGON);
	v = p->verts[0];
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		if ((ainpr_wall_mode.value==WALL_TEXTURE)||(ainpr_wall2_mode.value==WALL_SKETCH))
			glTexCoord2f (v[3], v[4]);
		if (ainpr_wall_mode.value==WALL_TOON)
			glTexCoord1f (v[3] * v[4]);
		glVertex3fv (v);
	}
	glEnd ();

	if(ainpr_line_mode.value!=LINE_NONE)
	{
		if (ainpr_line_mode.value!=LINE_TEXTURE)
		{
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_TEXTURE_1D);
		}

		glLineWidth(ainpr_line_width.value);
		setColor(getColorValue("line."));
		glBegin(GL_LINE_LOOP);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			if (ainpr_line_mode.value==LINE_TEXTURE)
				glTexCoord2f (v[3], v[4]);
			glVertex3fv (v);
		}
		glEnd ();
	}

	glPopAttrib();

}




/*
================
R_BlendLightmaps
================
*/
//adapted from sketch NPR Quake (took out randomness)
EXPORT void R_BlendLightmaps ( glpoly_t ** lightmap_polys, int lightmap_textures,
                       qboolean * lightmap_modified, glRect_t * lightmap_rectchange,
                       byte * lightmaps, int lightmap_bytes, int gl_lightmap_format,
                       cvar_t * r_lightmap, cvar_t * gl_texsort, cvar_t * r_fullbright,
                       double realtime)
{
	int			i, j, k;
	glpoly_t	*p;
	float		*v;

	if (ainpr_wall_mode.value==WALL_FLAT) return;
	
	if (r_fullbright->value)
		return;
	if (!gl_texsort->value)
		return;

	glDepthMask (0);		// don't bother writing Z

	if (gl_lightmap_format == GL_LUMINANCE)
		glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	else if (gl_lightmap_format == GL_INTENSITY)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4f (0,0,0,1);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	if (!r_lightmap->value)
	{
		glEnable (GL_BLEND);
	}

	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
		p = lightmap_polys[i];
		if (!p)
			continue;
		dr_GL_Bind(lightmap_textures+i);

		for ( ; p ; p=p->chain)
		{
			if (p->flags & SURF_UNDERWATER) {
			//	DrawGLWaterPolyLightmap( p, realtime );
			}
			else
			{
				if (ainpr_wall_mode.value==WALL_SKETCH)
				{
					glBlendFunc (GL_ZERO,GL_ONE_MINUS_SRC_COLOR);
					glEnable(GL_TEXTURE_2D);
				}
				glBegin (GL_POLYGON);
				v = p->verts[0];
				for (j=0 ; j<p->numverts ; j++, v+= VERTEXSIZE)
				{
					glTexCoord2f (v[5], v[6]);
					glVertex3fv (v);
				}
				glEnd ();
				
				if (ainpr_wall_mode.value==WALL_SKETCH)
				{
					CurRand = 0;

					glColor4f(0.0f, 0.0f, 0.0f, ainpr_alpha.value) ;
	
					glBlendFunc (GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
					glDisable(GL_TEXTURE_2D);
					for (j=0; j < ainpr_walls.value; j++)
					{
						glBegin (GL_LINE_LOOP) ;

						v = p->verts[0];
						for (k=0 ; k<p->numverts ; k++, v+= VERTEXSIZE)
						{
							glVertex3f (v[0] + randFloat (-1.0f, 1.0f), v[1] + randFloat (-1.0f, 1.0f), v[2] + randFloat (-1.0f, 1.0f)) ;
						}
						glEnd ();
					}
				}
			}
		}
	}

	glDisable (GL_BLEND);
	if (gl_lightmap_format == GL_LUMINANCE)
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	else if (gl_lightmap_format == GL_INTENSITY)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor4f (1,1,1,1);
	}

	glDepthMask (1);		// back to normal Z buffering
}







/*
===============
R_DrawParticles
===============
*/
//extern	cvar_t	sv_gravity;

EXPORT void R_DrawParticles (particle_t ** active_particles, particle_t ** free_particles,
                      int * ramp1, int * ramp2, int * ramp3, cvar_t * sv_gravity,
                      double cloldtime, double cltime, int particletexture,
                      vec3_t vright, vec3_t vup, unsigned int * d_8to24table,
                      vec3_t vpn, vec3_t r_origin)
{
	particle_t		*p, *kill;
	float			grav;
	int				i;
	float			time2, time3;
	float			time1;
	float			dvel;
	float			frametime;
	
	vec3_t			up, right;
	float			scale;

    dr_GL_Bind(particletexture);
	glEnable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBegin (GL_TRIANGLES);

	dr_VectorScale (vup, 1.5, up);
	dr_VectorScale (vright, 1.5, right);

    frametime = cltime - cloldtime;
	time3 = frametime * 15;
	time2 = frametime * 10; // 15;
	time1 = frametime * 5;
	grav = frametime * sv_gravity->value * 0.05;
	dvel = 4*frametime;
	
	for ( ;; ) 
	{
		kill = (*active_particles);
		if (kill && kill->die < cltime)
		{
			(*active_particles) = kill->next;
			kill->next = (*free_particles);
			(*free_particles) = kill;
			continue;
		}
		break;
	}

	for (p=(*active_particles) ; p ; p=p->next)
	{
		for ( ;; )
		{
			kill = p->next;
			if (kill && kill->die < cltime)
			{
				p->next = kill->next;
				kill->next = (*free_particles);
				(*free_particles) = kill;
				continue;
			}
			break;
		}

		// hack a scale up to keep particles from disapearing
		scale = (p->org[0] - r_origin[0])*vpn[0] + (p->org[1] - r_origin[1])*vpn[1]
			+ (p->org[2] - r_origin[2])*vpn[2];
		if (scale < 20)
			scale = 1;
		else
			scale = 1 + scale * 0.004;

		
        
        glColor3ubv ((byte *)&(d_8to24table[(int)p->color]));
        

		glTexCoord2f (0,0);
		glVertex3fv (p->org);
		glTexCoord2f (1,0);
		glVertex3f (p->org[0] + up[0]*scale, p->org[1] + up[1]*scale, p->org[2] + up[2]*scale);
		glTexCoord2f (0,1);
		glVertex3f (p->org[0] + right[0]*scale, p->org[1] + right[1]*scale, p->org[2] + right[2]*scale);

        p->org[0] += p->vel[0]*frametime;
		p->org[1] += p->vel[1]*frametime;
		p->org[2] += p->vel[2]*frametime;
		
        

		switch (p->type)
		{
		case pt_static:
			break;
		case pt_fire:
			p->ramp += time1;
			if (p->ramp >= 6)
				p->die = -1;
			else
				p->color = ramp3[(int)p->ramp];
			p->vel[2] += grav;
			break;

		case pt_explode:
			p->ramp += time2;
			if (p->ramp >=8)
				p->die = -1;
			else
				p->color = ramp1[(int)p->ramp];
			for (i=0 ; i<3 ; i++)
				p->vel[i] += p->vel[i]*dvel;
			p->vel[2] -= grav;
			break;

		case pt_explode2:
			p->ramp += time3;
			if (p->ramp >=8)
				p->die = -1;
			else
				p->color = ramp2[(int)p->ramp];
			for (i=0 ; i<3 ; i++)
				p->vel[i] -= p->vel[i]*frametime;
			p->vel[2] -= grav;
			break;

		case pt_blob:
			for (i=0 ; i<3 ; i++)
				p->vel[i] += p->vel[i]*dvel;
			p->vel[2] -= grav;
			break;

		case pt_blob2:
			for (i=0 ; i<2 ; i++)
				p->vel[i] -= p->vel[i]*dvel;
			p->vel[2] -= grav;
			break;

		case pt_grav:

        case pt_slowgrav:
			p->vel[2] -= grav;
			break;
		}
        
	}

	glEnd ();
	glDisable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

}






/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
//adapted from sketch NPR Quake (took out randomness)
EXPORT void EmitWaterPolys (msurface_t *fa, double realtime)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	int			j;
	float		s, t, os, ot;

	glDisable(GL_TEXTURE_2D) ;
	glEnable(GL_BLEND) ;
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) ;
	glDepthMask( 1 );

	glColor4f (0.34f,0.52f,0.75f, 1.0f) ;

	for (p=fa->polys ; p ; p=p->next)
	{
		glBegin (GL_POLYGON);
		for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			glVertex3fv (v);
		}
		glEnd ();
	}

	glDepthMask( 0 );

	for (p=fa->polys ; p ; p=p->next)
	{
		CurRand = 0;

		//glColor4f (0.64f,0.82f,0.95f,ainpr_alpha.value);
		glColor4f (0.0f,0.0f,0.0f,ainpr_alpha.value);

		glLineWidth(ainpr_line_width.value) ;
		glBegin (GL_LINE_LOOP);
		for (j=0; j < ainpr_water.value; j++)
		{
			for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
			{
				os = v[0];
				ot = v[1];

				s = 15 * turbsin[(int)((ot*0.125+realtime) * 300) & 255];
				s *= (1.0/64);

				t = 15 * turbsin[(int)((os*0.125+realtime) * 300) & 255];
				t *= (1.0/64);
			
				glVertex3f (v[0]+s+randFloat(-1.0,1.0), v[1]+t+randFloat(-1.0,1.0), v[2]+randFloat(-1.0,1.0)) ;
			}
			glLineWidth(1) ;
		}
		glEnd ();
	}

	glLineWidth(1.0) ;
	glDepthMask (1);
	glDisable(GL_BLEND) ;
	glEnable(GL_TEXTURE_2D) ;
}




/*
=============
EmitSkyPolys
=============
*/
EXPORT void EmitSkyPolys (msurface_t *fa, float speedscale, vec3_t r_origin)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float	s, t;
	vec3_t	dir;
	float	length;

	for (p=fa->polys ; p ; p=p->next)
	{
		glBegin (GL_POLYGON);
		for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			VectorSubtract (v, r_origin, dir);
			dir[2] *= 3;	// flatten the sphere

			length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
			length = sqrt (length);
			length = 6*63/length;

			dir[0] *= length;
			dir[1] *= length;

			s = (speedscale + dir[0]) * (1.0/128);
			t = (speedscale + dir[1]) * (1.0/128);

			glTexCoord2f (s, t);
			glVertex3fv (v);
		}
		glEnd ();
	}
}