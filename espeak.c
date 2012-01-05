/******************************************************
 *
 * espeak 0.2- implementation file
 *
 * (c) copyright 2011 Matthias Kronlachner
 * (c) copyright 2011 IOhannes m zmölnig
 *
 * now based on flite external by Bryan Jurish -> write to array
 * 
 *  within the course: Künstlerisches Gestalten mit Klang 2010/2011
 *
 *   institute of electronic music and acoustics (iem)
 *
 ******************************************************
 *
 * license: GNU General Public License v.2
 *
 ******************************************************/

#include "m_pd.h"
#include <espeak/speak_lib.h>
#include <string.h>

#include <math.h>

#ifdef __GNUC__
# define MOO_UNUSED __attribute__((unused))
#else
# define MOO_UNUSED
#endif

/*-- PDEXT_UNUSED : alias for MOO_UNUSED --*/
#define PDEXT_UNUSED MOO_UNUSED

/*--------------------------------------------------------------------
 * Globals
 *--------------------------------------------------------------------*/

static int espeak_rate=0;
#define ESPEAK_BUFFER 50000

#define DEFAULT_BUFSIZE 256
#define DEFAULT_BUFSTEP 256


/*---------------------------------------------------------------------
 * espeak
 *---------------------------------------------------------------------*/

typedef struct _espeak
{
  t_object x_obj;                    /* black magic (probably inheritance-related) */
  t_symbol *x_arrayname;             /* arrayname (from '_tabwrite' code in $PD_SRC/d_array.c) */
  char     *textbuf;                 /* text buffer (hack) */
  
  int      bufsize;         /* text buffer size */
  short*x_buffer;        		/* buffer of synth'ed samples */
  unsigned int x_buflen; 		/* length of the buffer */
  unsigned int x_position; 	/* playback position for perform-routine */
  int x_new;           		/* flag: if true, start playback */

  t_outlet*x_infoout;		
} t_espeak;

static t_class *espeak_class;

/*--------------------------------------------------------------------
 * espeak_synth : synthesize current text-buffer
 *--------------------------------------------------------------------*/
void espeak_synth(t_espeak *x) {
	
  t_garray *a;
  
  // -- sanity checks
  if (!(a = (t_garray *)pd_findbyclass(x->x_arrayname, garray_class))) {
    pd_error(x,"espeak:: no such array '%s'", x->x_arrayname->s_name);
    return;
  }
  
  if (!strlen(x->textbuf)) {
    pd_error(x,"espeak:: attempt to synthesize empty text-buffer!");
    return;
  }
  
  verbose(1, "espeak: strlen textbuffer %d",strlen(x->textbuf));

  
  verbose(1, "espeak: call Synth function");


// SYNTHESIZE
	 x->x_new=1;
	 
  espeak_Synth(x->textbuf,
	       strlen(x->textbuf),
	       0,
	       POS_CHARACTER,
	       0,
	       espeakCHARS_AUTO,
	       NULL,
	       x);

}

/*--------------------------------------------------------------------
 * espeak_callback : callback function
 *--------------------------------------------------------------------*/
static int espeak_callback(short *wav, int numsamples, espeak_EVENT *events) {
  int i=0;
  t_garray *a;
  t_float *vec;
  int vecsize;

	t_espeak*x = (t_espeak*)events[0].user_data;
	
  verbose(1, "got %d samples", numsamples);
  

	int eventnr = (int)events[0].type;
	
	verbose(1, "event number %i \n",eventnr);
	

		// Link Array
		if (!(a = (t_garray *)pd_findbyclass(x->x_arrayname, garray_class))) {
			pd_error(x,"espeak: no such array '%s'", x->x_arrayname->s_name);
		}
		
		// eventnr 6 --> terminate
		if (eventnr==6)
		{
			// OUTPUT BUFFER LENGTH WHEN FINISHED
			t_atom ap[1];
			SETFLOAT(ap, x->x_position);
			outlet_anything(x->x_infoout, gensym("length"), 1, ap);
			garray_redraw(a);
		} else {
			
			if (numsamples)
			{
				if (x->x_new==1) // NEW SYNTH
				{
					verbose(1, "espeak: array initialized - garray_resize(%d)", numsamples);

					garray_resize(a, numsamples); //Resize from zero
					
					if (!garray_getfloatarray(a, &vecsize, &vec))
						pd_error(x,"espeak: bad template for write to array '%s'", x->x_arrayname->s_name);
					x->x_position=0; //set array position
					x->x_new=0;
				} else {
					
					garray_resize(a, numsamples+x->x_position); //Resize for new received samples
					if (!garray_getfloatarray(a, &vecsize, &vec))
							pd_error(x,"espeak: bad template for write to array '%s'", x->x_arrayname->s_name);
							
					verbose(1, "espeak: garray_resize(%d)", numsamples+vecsize);
					
				}
				
				//verbose(1, "espeak: copy buffer (%d)", numsamples);
				//memset(x->x_buffer, 0, x->x_buflen*sizeof(short));
				//memcpy(x->x_buffer, wav, numsamples*sizeof(short));
    
				verbose(1, "espeak: writing to buffer position: %d", x->x_position);

				for (i = 0; i < numsamples; i++) {
					//verbose(1, "espeak: %d", i+x->x_position);
					vec[i+x->x_position] = (wav[i])/32767.0;
				}
				
				x->x_position+=numsamples;
			
		}
	}
	
  return 0;
}

/*--------------------------------------------------------------------
 * espeak_text : set text-buffer
 *--------------------------------------------------------------------*/
void espeak_text(t_espeak *x, MOO_UNUSED t_symbol *s, int argc, t_atom *argv) {
  int i, alen, buffered;
  t_symbol *asym;

  // -- allocate initial text-buffer if required
  if (x->textbuf == NULL) {
    x->bufsize = DEFAULT_BUFSIZE;
    x->textbuf = getbytes(x->bufsize);
  }
  if (x->textbuf == NULL) {
    pd_error(x,"espeak: allocation failed for text buffer");
    x->bufsize = 0;
    return;
  }

  // -- convert args to strings
  buffered = 0;
  for (i = 0; i < argc; i++) {
    asym = atom_gensym(argv);
    alen = 1+strlen(asym->s_name);

    // -- reallocate if necessary
    while (buffered + alen > x->bufsize) {
      x->textbuf = resizebytes(x->textbuf,x->bufsize,x->bufsize+DEFAULT_BUFSTEP);
      x->bufsize = x->bufsize+DEFAULT_BUFSTEP;
      if (x->textbuf == NULL) {
	pd_error(x,"espeak: allocation failed for text buffer");
	x->bufsize = 0;
	return;
      }
    }
    
    // -- append arg-string to text-buf
    if (i == 0) {
      strcpy(x->textbuf+buffered, asym->s_name);
      buffered += alen-1;
    } else {
      *(x->textbuf+buffered) = ' ';
      strcpy(x->textbuf+buffered+1, asym->s_name);
      buffered += alen;
    }
    
    // -- increment/decrement
    argv++;
  }


  post("espeak: got text='%s'", x->textbuf);

}

/*--------------------------------------------------------------------
 * espeak_voice : set voice
 *--------------------------------------------------------------------*/
static void espeak_voice(t_espeak*x, t_symbol*s){
  espeak_ERROR err= espeak_SetVoiceByName(s->s_name);
  if((int)err==0)
  {
		post("espeak:: set voice to='%s'", s->s_name);
  } else {
		post("espeak:: no voice named '%s'", s->s_name);
	}
}

/*--------------------------------------------------------------------
 * espeak_info : output possible voices
 *--------------------------------------------------------------------*/
static void espeak_info(t_espeak*x){
  const espeak_VOICE**voices=espeak_ListVoices(NULL);
  int i=0;

  while(voices[i]) {
    
    t_atom ap[3];
    SETSYMBOL(ap+0, gensym(voices[i]->name));
    SETSYMBOL(ap+1, gensym(voices[i]->languages));
    SETSYMBOL(ap+2, gensym(voices[i]->identifier));

    outlet_anything(x->x_infoout, gensym("voice"), 3, ap);

    i++;
  }
}

/*--------------------------------------------------------------------
 * espeak_voice : set word rate
 *--------------------------------------------------------------------*/
static void espeak_wordrate(t_espeak*x, t_float f){
  espeak_ERROR err = espeak_SetParameter(espeakRATE, (int) f, 0);
  if((int)err==0)
  {
		post("espeak:: set wordrate to='%d'", (int) f);
  } else {
		post("espeak:: no voice named '%d'", (int) f);
	}
}

/*--------------------------------------------------------------------
 * espeak_voice : set pitch range
 *--------------------------------------------------------------------*/
static void espeak_range(t_espeak*x, t_float f){
  if(f<0)
  {
		f=0;
  } else if (f>100)
  {
		f=100;
  }
  espeak_ERROR err = espeak_SetParameter(espeakRANGE, (int) f, 0);
  if((int)err==0)
  {
		post("espeak:: set pitch range to='%d'", (int) f);
  } else {
		post("espeak:: error setting pitch range '%d'", (int) f);
	}
}

/*--------------------------------------------------------------------
 * espeak_voice : set pitch
 *--------------------------------------------------------------------*/
static void espeak_pitch(t_espeak*x, t_float f){
		if(f<0)
		{
			f=0;
		} else if (f>100)
		{
			f=100;
		}
		espeak_ERROR err = espeak_SetParameter(espeakPITCH, (int) f, 0);
		if((int)err==0)
		{
			post("espeak:: set pitch to='%d'", (int) f);
		} else {
			post("espeak:: error setting pitch '%d'", (int) f);
		}
}
static void espeak_pitch_out(t_espeak*x){
	int parameter = espeak_GetParameter(espeakPITCH, 1);
	t_atom ap[1];
	SETFLOAT(ap, parameter);
	outlet_anything(x->x_infoout, gensym("pitch"), 1, ap);
}
/*--------------------------------------------------------------------
 * espeak_list : parse & synthesize text in one swell foop
 *--------------------------------------------------------------------*/
void espeak_list(t_espeak *x, t_symbol *s, int argc, t_atom *argv) {
  espeak_text(x,s,argc,argv);
  espeak_synth(x);
}


/*--------------------------------------------------------------------
 * espeak_set : set arrayname
 *--------------------------------------------------------------------*/
static void espeak_set(t_espeak *x, t_symbol *ary) {

  post("espeak: called with array='%s'", ary->s_name);

  x->x_arrayname = ary;
}


/*--------------------------------------------------------------------
 * constructor / destructor
 *--------------------------------------------------------------------*/
static void *espeak_new(t_symbol *ary)
{
  t_espeak *x;

  x = (t_espeak *)pd_new(espeak_class);

	x->x_new=0;
	 
  // set initial arrayname
  x->x_arrayname = ary;

  // init buffer
  x->x_buflen = espeak_rate*ESPEAK_BUFFER/1000;
  x->x_buffer = (short*)getbytes(x->x_buflen*sizeof(short));

  // init textbuf
  x->textbuf = NULL;
  x->bufsize = 0;
  
  // create info outlet
  x->x_infoout=outlet_new(&x->x_obj, NULL);

  return (void *)x;
}

static void espeak_free(t_espeak *x) {
  if (x->bufsize && x->textbuf != NULL) {
    freebytes(x->textbuf, x->bufsize);
    freebytes(x->x_buffer, x->x_buflen*sizeof(short));
    x->bufsize = 0;
  }
}

/*--------------------------------------------------------------------
 * setup
 *--------------------------------------------------------------------*/
void espeak_setup(void) {
	
	//INIT ESPEAK
  int result=espeak_Initialize(AUDIO_OUTPUT_RETRIEVAL, 
			       ESPEAK_BUFFER,
			       NULL,
			       0);


  if(result<0) {
    error("couldn't initialize eSpeak");
    return;
  }

  espeak_SetSynthCallback(espeak_callback);
  espeak_rate=result;

  post("espeak: eSpeak running at %d Hz samplingrate", result);
  
			
  // --- register class
  espeak_class = class_new(gensym("espeak"),
			  (t_newmethod)espeak_new,  // newmethod
			  (t_method)espeak_free,    // freemethod
			  sizeof(t_espeak),         // size
			  CLASS_DEFAULT,           // flags
			  A_DEFSYM,                // arg1: table-name
			  0);

  // --- class methods
  class_addlist(espeak_class, espeak_list);
  class_addmethod(espeak_class, (t_method)espeak_set,   gensym("set"),   A_DEFSYM, 0);
  class_addmethod(espeak_class, (t_method)espeak_text,  gensym("text"),  A_GIMME, 0);
  class_addmethod(espeak_class, (t_method)espeak_synth, gensym("synth"), 0);
  class_addmethod(espeak_class, (t_method)espeak_voice, gensym("voice"), A_SYMBOL, 0);
	class_addmethod(espeak_class, (t_method)espeak_wordrate, gensym("rate"), A_FLOAT, 0);
	class_addmethod(espeak_class, (t_method)espeak_range, gensym("range"), A_FLOAT, 0);
	class_addmethod(espeak_class, (t_method)espeak_pitch, gensym("pitch"), A_FLOAT, 0);
	//class_addmethod(espeak_class, (t_method)espeak_pitch_out, gensym("pitch"), 0);
	
  class_addbang(espeak_class, (t_method)espeak_info);
}
