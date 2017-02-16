#ifndef __HOTSPOT_H_
#define __HOTSPOT_H_

#include "util.h"
#include "flp.h"
#include "package.h"
#include "temperature.h"
#include "temperature_block.h"
#include "temperature_grid.h"

char **alloc_names(int nr, int nc);
void free_names(char **m);

/* global configuration parameters for HotSpot	*/
typedef struct global_config_t_st
{
	/* floorplan input file */
	char flp_file[STR_SIZE];
	/* input power trace file */
	char p_infile[STR_SIZE];
	/* output file for the temperature trace */
	char t_outfile[STR_SIZE];
	/* input configuration parameters from file	*/
	char config[STR_SIZE];
	/* output configuration parameters to file	*/
	char dump_config[STR_SIZE];

	
	/*BU_3D: Option to turn on heterogenous R-C assignment*/
	char detailed_3D[STR_SIZE];
	
}global_config_t;

/* 
 * parse a table of name-value string pairs and add the configuration
 * parameters to 'config'
 */
void global_config_from_strs(global_config_t *config, str_pair *table, int size);
/* 
 * convert config into a table of name-value pairs. returns the no.
 * of parameters converted
 */
int global_config_to_strs(global_config_t *config, str_pair *table, int max_entries);

class Hotspot
{
public:
Hotspot();
~Hotspot();

/*
* global variables used in initialization 
*/

int num, size, lines = 0, do_transient = TRUE;
/* floorplan	*/
flp_t *flp;
/* hotspot temperature model	*/
RC_model_t *model;

/* thermal model configuration parameters	*/
thermal_config_t thermal_config;
/* global configuration parameters	*/
global_config_t global_config;
/* table to hold options and configuration */
str_pair table[MAX_ENTRIES];

int do_detailed_3D = FALSE; //BU_3D: do_detailed_3D, false by default
/*
* end of global variables 
*/


/* 
 * called by sniper to calculate temperature
 * need to read init_file, core_layer.flr, power_trace, -model_type grid, -grid_layer_file test_3D.lcf, -grid_steady_file..
 * example: ($1)fft-small ($2)StackedDramCache.input ($3)cache.steady ($4)cache.ttrace ($5)cache.grid.steady 
 *          ../hotspot -c ../hotspot.config -f core_layer.flr -p $1/$2 -steady_file $1/$3 -model_type grid -grid_layer_file test_3D.lcf
 *          ../hotspot -c ../hotspot.config -init_file $1/$3 -f core_layer.flr -p $1/$2 -o $1/$4 -model_type grid -grid_layer_file test_3D.lcf -grid_steady_file $1/$5
 */
void getNames(const char *file, char **names, int *len);
void initHotSpot(int argc, char **argv);
void calculateTemperature(double *temp_rst, int argc, char **argv);
void endHotSpot();
};

#endif
