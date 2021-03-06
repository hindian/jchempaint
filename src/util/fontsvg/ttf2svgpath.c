#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#define BATIKTTF2SVG "java -jar /usr/share/batik-1.7/lib/batik-ttf2svg.jar"

void usage()
{
	printf("Usage:    ttf2svgpath fontfile.ttf\n");
}

int numC=0;
typedef struct {
	char *str;
	unsigned int wchar;
	int xMin, xMax, yMin, yMax;
	unsigned int adv;
	} GMET;
GMET *pa;

wint_t* read_characters()
{
	setlocale (LC_ALL, "en_US.UTF-8");
	char *filename = "characters.txt";
	FILE *infile;
	infile = fopen (filename, "r, ccs=UTF-8");
	printf ("File orientation: %d\n", fwide (infile,0));
	static wint_t b[16384], c, *p;
	p = b;
	while ((p-b)<sizeof(b)-4 && (c = fgetwc (infile)) != WEOF)
		{ *p++ = c; printf("%04X ", c); }
	if (c == WEOF) printf (" WEOF \n");
	*p++ = WEOF;
	printf("\nRead %ld wint_t chars from characters.txt\n", p-b-1);
	printf ("File orientation: %d\n", fwide (infile,0));

	numC = p-b-1;
	pa = malloc (numC*sizeof(GMET));
	return b;
}

int Min=9999, Max=0;

void load_ttfont (wint_t *chrnums, char *fname)
{
	FT_Library ftlib;
	int error = FT_Init_FreeType( &ftlib );
	if ( error ) {
		printf("an error occurred during library initialization!\n"); 
	}
	FT_Face face;
	error = FT_New_Face( ftlib, fname, 0, &face );
	if ( error == FT_Err_Unknown_File_Format )
	{ printf("the font file could be opened and read, but it appears ... that its font format is unsupported\n"); }
	else if ( error ) { printf("the font file could not be opened or read, or it is broken...\n"); }

	printf("Number of glyphs in font: %ld\n", face->num_glyphs);
	printf("This font is scalable: %s\n",
		face->face_flags&FT_FACE_FLAG_SCALABLE? "yes":"no");
	printf("This face uses 'sfnt' storage: %s\n",
		face->face_flags&FT_FACE_FLAG_SFNT? "yes":"no");
	printf("This face contains kerning: %s\n",
		face->face_flags&FT_FACE_FLAG_KERNING? "yes":"no");
	printf("This font contains glyph names: %s\n",
		face->face_flags&FT_FACE_FLAG_GLYPH_NAMES? "yes":"no");
	printf("This font is CID-keyed: %s\n\n",
		face->face_flags&FT_FACE_FLAG_CID_KEYED? "yes":"no");

	printf("Units per EM: %d\n", face->units_per_EM);
	printf("Current height is: %ld\n", face->glyph->metrics.height);
	
	int k=-1, pa_idx=0;
	while (chrnums[++k] > 0)
	{
		wint_t c = chrnums[k];
		if (c == WEOF) break;
		if (c == 10) continue;
		pa[pa_idx].wchar = c;
		if (c > Max) Max = c;
		if (c < Min) Min = c;
		FT_UInt i = FT_Get_Char_Index (face, c);
		if (i == 0) { printf("FT_Get_Char_Index() returned 0\n"); exit(1); }
		char name[128];
		error = FT_Get_Glyph_Name (face, i, name, 127);
		pa[pa_idx].str = malloc(4+sizeof(char)*strlen(name));
		strcpy (pa[pa_idx].str, name);
		printf ("%04d: %04d-'%s' ", chrnums[k], i, name);
	
		FT_Glyph glyph;
		int flags = FT_LOAD_NO_SCALE;
		error = FT_Load_Glyph( face, i, FT_LOAD_NO_SCALE );
		if ( error ) { printf("Couldn't FT_Load_glyph()\n"); exit(1); }
		error = FT_Get_Glyph( face->glyph, &glyph );
		if ( error ) { printf("Couldn't FT_Get_glyph()\n"); exit(1); }

		FT_OutlineGlyph *ogp = NULL;
		if (glyph->format == FT_GLYPH_FORMAT_OUTLINE)
			ogp = (FT_OutlineGlyph*) &glyph;
		FT_Outline ol = (*ogp)->outline;
		FT_BBox bbox;
		FT_Outline_Get_CBox (&ol, &bbox);
		printf("(%ld, %ld, %ld, %ld) ",
			bbox.xMin, bbox.xMax,
			bbox.yMin, bbox.yMax);
		pa[pa_idx].xMin = bbox.xMin;
		pa[pa_idx].xMax = bbox.xMax;
		pa[pa_idx].yMin = bbox.yMin;
		pa[pa_idx].yMax = bbox.yMax;

		FT_Pos adv = face->glyph->metrics.horiAdvance;
		printf("%ld\n", adv);
		pa[pa_idx].adv = adv;
		++pa_idx;
	}
	numC = pa_idx;
}
  
char *cFile = NULL;
size_t flen;

void load_svgfont (char *fname)
{
	FILE *fp = fopen("FreeSansBold.svg", "r");
	if (fp == NULL )             /* Could not open file */
	{ printf("Error opening %s\n", fname); exit(1);	}

	fseek(fp, 0L, SEEK_END);     /* Position to end of file */
	flen = ftell(fp);        /* Get file length */
	rewind(fp);                  /* Back to start of file */
	cFile = malloc ((flen+8)*sizeof(char));
	size_t len = fread (cFile, 1, flen, fp);
	fclose(fp);
}

void write_data (char *fname)
{
	char *s = strrchr (fname, '/');
	char *p = strchr (fname, '.');
	if (s == NULL) s=fname;
	char fn[128], jfn[256], cn[256];
	strncpy (fn, s, p-s);
	fn[p-s] = '\0';
	sprintf (jfn, "%sGM.java", fn);
	sprintf (cn, "%sGM", fn);

	FILE *fp = fopen (jfn, "w");
	if (fp == NULL)
	{ printf("Error opening %s\n", fname); exit(1); }

	fprintf (fp, "/* %s: Autogenerated by ttf2svgpath from %s */\n\n", jfn, fname);
	fprintf (fp, "package fontsvg;\n\n");
	fprintf (fp, "import java.util.HashMap;\n");
	fprintf (fp, "import java.util.Map;\n\n");
	fprintf (fp, "public class %s {\n\n", cn);
	fprintf (fp, "    public class Metric {\n\n");
	fprintf (fp, "        public int xMin, xMax, yMin, yMax, adv;\n");
	fprintf (fp, "        public String outline;\n");
	fprintf (fp, "        public Metric (int a, int b, int c, int d, int e, String s) {\n");
	fprintf (fp, "            xMin=a; xMax=b; yMin=c; yMax=d; adv=e; outline=s;\n");
	fprintf (fp, "        }\n\n");
	fprintf (fp, "    }\n");
	fprintf (fp, "public static Map<Integer, Metric> map;\n\n");
	fprintf (fp, "public static %s dummy;\n\n", cn);
	fprintf (fp, "static {\n");
	fprintf (fp, "    map = new HashMap<Integer, Metric>();\n");

	for (int i=0; i<numC; i++)
	{
		GMET *pp = &pa[i];
		char *p, *cp = cFile;
		while ((p = strstr (cp, "glyph-name=\"")) != NULL)
		{
			p =  cp = p+12;
			if (!strncmp (p, pp->str, strlen(pp->str))) break;
		}
		if (p == NULL)
		{ printf ("Glyph %d (%s) not found.\n", pp->wchar, pp->str); exit(1); }

		p = strstr (p, "d=\"") + 3;
		char *eol = strstr (p, "\" />");
		fprintf (fp, "    map.put (0x%x, dummy.new Metric(%d, %d, %d, %d, %d, \"",
			pp->wchar, pp->xMin,
			pp->xMax, pp->yMin,
			pp->yMax, pp->adv);
		--p;
		while (p++ < eol-1) {
			if (*p==0x0a) continue;
			else fputc (*p, fp); 
			}
		fprintf(fp, "\"));\n");
	}
	fprintf (fp, "};\n");
	fprintf (fp, "}\n\n");

	fclose (fp);
}

int main(int argc, char* argv[])
{
	if (argc != 2) { usage(); return 1; }

	wint_t* wchars = read_characters();
	load_ttfont (wchars, argv[1]);
	load_svgfont (argv[1]);
	write_data (argv[1]);
}
