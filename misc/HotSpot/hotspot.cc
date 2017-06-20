/* 
 * This is a trace-level thermal simulator. It reads power values 
 * from an input trace file and outputs the corresponding instantaneous 
 * temperature values to an output trace file. It also outputs the steady 
 * state temperature values to stdout.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "hotspot.h"

/* HotSpot thermal model is offered in two flavours - the block
 * version and the grid version. The block model models temperature
 * per functional block of the floorplan while the grid model
 * chops the chip up into a matrix of grid cells and models the 
 * temperature of each cell. It is also capable of modeling a
 * 3-d chip with multiple floorplans stacked on top of each
 * other. The choice of which model to choose is done through
 * a command line or configuration file parameter model_type. 
 * "-model_type block" chooses the block model while "-model_type grid"
 * chooses the grid model. 
 */

/* Guidelines for choosing the block or the grid model	*/

/**************************************************************************/
/* HotSpot contains two methods for solving temperatures:                 */
/* 	1) Block Model -- the same as HotSpot 2.0	              						  */
/*	2) Grid Model -- the die is divided into regular grid cells       	  */
/**************************************************************************/
/* How the grid model works: 											                        */
/* 	The grid model first reads in floorplan and maps block-based power	  */
/* to each grid cell, then solves the temperatures for all the grid cells,*/
/* finally, converts the resulting grid temperatures back to block-based  */
/* temperatures.                            														  */
/**************************************************************************/
/* The grid model is useful when 				                    						  */
/* 	1) More detailed temperature distribution inside a functional unit    */
/*     is desired.														                            */
/*  2) Too many functional units are included in the floorplan, resulting */
/*		 in extremely long computation time if using the Block Model        */
/*	3) If temperature information is desired for many tiny units,		      */ 
/* 		 such as individual register file entry.						                */
/**************************************************************************/
/*	Comparisons between Grid Model and Block Model:						            */
/*		In general, the grid model is more accurate, because it can deal    */
/*	with various floorplans and it provides temperature gradient across	  */
/*	each functional unit. The block model models essentially the center	  */
/*	temperature of each functional unit. But the block model is typically */
/*	faster because there are less nodes to solve.						              */
/*		Therefore, unless it is the case where the grid model is 		        */
/*	definitely	needed, we suggest using the block model for computation  */
/*  efficiency.															                              */
/**************************************************************************/

void usage(int argc, char **argv)
{
  fprintf(stdout, "Usage: %s -f <file> -p <file> [-o <file>] [-c <file>] [-d <file>] [options]\n", argv[0]);
  fprintf(stdout, "A thermal simulator that reads power trace from a file and outputs temperatures.\n");
  fprintf(stdout, "Options:(may be specified in any order, within \"[]\" means optional)\n");
  fprintf(stdout, "   -f <file>\tfloorplan input file (e.g. ev6.flp) - overridden by the\n");
  fprintf(stdout, "            \tlayer configuration file (e.g. layer.lcf) when the\n");
  fprintf(stdout, "            \tlatter is specified\n");
  fprintf(stdout, "   -p <file>\tpower trace input file (e.g. gcc.ptrace)\n");
  fprintf(stdout, "  [-o <file>]\ttransient temperature trace output file - if not provided, only\n");
  fprintf(stdout, "            \tsteady state temperatures are output to stdout\n");
  fprintf(stdout, "  [-c <file>]\tinput configuration parameters from file (e.g. hotspot.config)\n");
  fprintf(stdout, "  [-d <file>]\toutput configuration parameters to file\n");
  fprintf(stdout, "  [options]\tzero or more options of the form \"-<name> <value>\",\n");
  fprintf(stdout, "           \toverride the options from config file. e.g. \"-model_type block\" selects\n");
  fprintf(stdout, "           \tthe block model while \"-model_type grid\" selects the grid model\n");
  fprintf(stdout, "  [-detailed_3D <on/off]>\tHeterogeneous R-C assignments for specified layers. Requires a .lcf file to be specified\n"); //BU_3D: added detailed_3D option
}

/* 
 * parse a table of name-value string pairs and add the configuration
 * parameters to 'config'
 */
void global_config_from_strs(global_config_t *config, str_pair *table, int size)
{
  int idx;
  if ((idx = get_str_index(table, size, "f")) >= 0) {
      if(sscanf(table[idx].value, "%s", config->flp_file) != 1)
        fatal("invalid format for configuration  parameter flp_file\n");
  } else {
      fatal("required parameter flp_file missing. check usage\n");
  }
  if ((idx = get_str_index(table, size, "p")) >= 0) {
      if(sscanf(table[idx].value, "%s", config->p_infile) != 1)
        fatal("invalid format for configuration  parameter p_infile\n");
  } else {
      fatal("required parameter p_infile missing. check usage\n");
  }
  if ((idx = get_str_index(table, size, "o")) >= 0) {
      if(sscanf(table[idx].value, "%s", config->t_outfile) != 1)
        fatal("invalid format for configuration  parameter t_outfile\n");
  } else {
      strcpy(config->t_outfile, NULLFILE);
  }
  if ((idx = get_str_index(table, size, "c")) >= 0) {
      if(sscanf(table[idx].value, "%s", config->config) != 1)
        fatal("invalid format for configuration  parameter config\n");
  } else {
      strcpy(config->config, NULLFILE);
  }
  if ((idx = get_str_index(table, size, "d")) >= 0) {
      if(sscanf(table[idx].value, "%s", config->dump_config) != 1)
        fatal("invalid format for configuration  parameter dump_config\n");
  } else {
      strcpy(config->dump_config, NULLFILE);
  }
  if ((idx = get_str_index(table, size, "detailed_3D")) >= 0) {
      if(sscanf(table[idx].value, "%s", config->detailed_3D) != 1)	
        fatal("invalid format for configuration  parameter lc\n");
  } else {
      strcpy(config->detailed_3D, "off");
  }
}

/* 
 * convert config into a table of name-value pairs. returns the no.
 * of parameters converted
 */
int global_config_to_strs(global_config_t *config, str_pair *table, int max_entries)
{
  if (max_entries < 6)
    fatal("not enough entries in table\n");

  sprintf(table[0].name, "f");
  sprintf(table[1].name, "p");
  sprintf(table[2].name, "o");
  sprintf(table[3].name, "c");
  sprintf(table[4].name, "d");
  sprintf(table[5].name, "detailed_3D");
  sprintf(table[0].value, "%s", config->flp_file);
  sprintf(table[1].value, "%s", config->p_infile);
  sprintf(table[2].value, "%s", config->t_outfile);
  sprintf(table[3].value, "%s", config->config);
  sprintf(table[4].value, "%s", config->dump_config);
  sprintf(table[5].value, "%s", config->detailed_3D);

  return 6;
}

/* 
 * read a single line of trace file containing names
 * of functional blocks
 */
int read_names(FILE *fp, char **names)
{
  char line[LINE_SIZE], temp[LINE_SIZE], *src;
  int i;

  /* skip empty lines	*/
  do {
      /* read the entire line	*/
      fgets(line, LINE_SIZE, fp);
      if (feof(fp))
        fatal("not enough names in trace file\n");
      strcpy(temp, line);
      src = strtok(temp, " \r\t\n");
  } while (!src);

  /* new line not read yet	*/	
  if(line[strlen(line)-1] != '\n')
    fatal("line too long\n");

  /* chop the names from the line read	*/
  for(i=0,src=line; *src && i < MAX_UNITS; i++) {
      if(!sscanf(src, "%s", names[i]))
        fatal("invalid format of names\n");
      src += strlen(names[i]);
      while (isspace((int)*src))
        src++;
  }
  if(*src && i == MAX_UNITS)
    fatal("no. of units exceeded limit\n");

  return i;
}

/* read a single line of power trace numbers	*/
int read_vals(FILE *fp, double *vals)
{
  char line[LINE_SIZE], temp[LINE_SIZE], *src;
  int i;

  /* skip empty lines	*/
  do {
      /* read the entire line	*/
      fgets(line, LINE_SIZE, fp);
      if (feof(fp))
        return 0;
      strcpy(temp, line);
      src = strtok(temp, " \r\t\n");
  } while (!src);

  /* new line not read yet	*/	
  if(line[strlen(line)-1] != '\n')
    fatal("line too long\n");

  /* chop the power values from the line read	*/
  for(i=0,src=line; *src && i < MAX_UNITS; i++) {
      if(!sscanf(src, "%s", temp) || !sscanf(src, "%lf", &vals[i]))
        fatal("invalid format of values\n");
      src += strlen(temp);
      while (isspace((int)*src))
        src++;
  }
  if(*src && i == MAX_UNITS)
    fatal("no. of entries exceeded limit\n");

  return i;
}

/* write a single line of functional unit names	*/
void write_names(FILE *fp, char **names, int size)
{
  int i;
  for(i=0; i < size-1; i++)
    fprintf(fp, "%s\t", names[i]);
  fprintf(fp, "%s\n", names[i]);
}

/* write a single line of temperature trace(in degree C)	*/
void write_vals(FILE *fp, double *vals, int size)
{
  int i;
  for(i=0; i < size-1; i++)
    fprintf(fp, "%.2f\t", vals[i]-273.15);
  fprintf(fp, "%.2f\n", vals[i]-273.15);
}

char **alloc_names(int nr, int nc)
{
  int i;
  char **m;

  m = (char **) calloc (nr, sizeof(char *));
  assert(m != NULL);
  m[0] = (char *) calloc (nr * nc, sizeof(char));
  assert(m[0] != NULL);

  for (i = 1; i < nr; i++)
    m[i] =  m[0] + nc * i;

  return m;
}

void free_names(char **m)
{
  free(m[0]);
  free(m);
}

/*
* class of HotSpot
*/

Hotspot::Hotspot()
{
}

Hotspot::~Hotspot()
{
	endHotSpot();
}

void
Hotspot::getNames(const char *file, char **names, int *len)
{
	FILE *fp;
	if(!(fp = fopen(file, "r")))
		fatal("unable to open power trace input file\n");
	char line[LINE_SIZE], temp[LINE_SIZE], *src;
	int i;

	/* skip empty lines	*/
	do {
		/* read the entire line	*/
		fgets(line, LINE_SIZE, fp);
		if (feof(fp))
			fatal("not enough names in trace file\n");
		strcpy(temp, line);
		src = strtok(temp, " \r\t\n");
	} while (!src);

	 /* new line not read yet	*/	
	if(line[strlen(line)-1] != '\n')
		fatal("line too long\n");

	/* chop the names from the line read	*/
	for(i=0,src=line; *src && i < MAX_UNITS; i++) {
		if(!sscanf(src, "%s", names[i]))
			fatal("invalid format of names\n");
		src += strlen(names[i]);
		while (isspace((int)*src))
			src++;
	}
	if(*src && i == MAX_UNITS)
		fatal("no. of units exceeded limit\n");

	*len = i;
	fclose(fp);
}

/*
* init HotSpot
*/
void 
Hotspot::initHotSpot(int argc, char **argv)
{
  int i;
  printf("*[Hotspot] begin initialization!\n");

  n = 0; do_transient = TRUE; do_temp_init = TRUE;
  //first_call = TRUE;
  do_detailed_3D = FALSE;
  

  size = parse_cmdline(table, MAX_ENTRIES, argc, argv);
  global_config_from_strs(&global_config, table, size);

  /* no transient simulation, only steady state	*/
  /*
  if(!strcmp(global_config.t_outfile, NULLFILE))
    do_transient = FALSE;
	*/

  /* read configuration file	*/
  if (strcmp(global_config.config, NULLFILE))
    size += read_str_pairs(&table[size], MAX_ENTRIES, global_config.config);

  /* earlier entries override later ones. so, command line options 
   * have priority over config file 
   */
  size = str_pairs_remove_duplicates(table, size);

  /* BU_3D: check if heterogenous R-C modeling is on */
  if(!strcmp(global_config.detailed_3D, "on")){
      do_detailed_3D = TRUE;
  }
  else if(strcmp(global_config.detailed_3D, "off")){
      fatal("detailed_3D parameter should be either \'on\' or \'off\'\n");
  }//end->BU_3D

  /* get defaults */
  thermal_config = default_thermal_config();
  /* modify according to command line / config file	*/
  thermal_config_add_from_strs(&thermal_config, table, size);

  /* initialization: the flp_file global configuration 
   * parameter is overridden by the layer configuration 
   * file in the grid model when the latter is specified.
   */
  flp = read_flp(global_config.flp_file, FALSE);

  //BU_3D: added do_detailed_3D to alloc_RC_model. Detailed 3D modeling can only be used with grid-level modeling.
  /* allocate and initialize the RC model	*/
  model = alloc_RC_model(&thermal_config, flp, do_detailed_3D); 
  if (model->type == BLOCK_MODEL && do_detailed_3D) 
    fatal("Detailed 3D option can only be used with grid model\n"); //end->BU_3D
  if ((model->type == GRID_MODEL) && !model->grid->has_lcf && do_detailed_3D) 
    fatal("Detailed 3D option can only be used in 3D mode\n");

  populate_R_model(model, flp);

  populate_C_model(model, flp);

  init_temp = hotspot_vector(model);
  /* n is the number of functional blocks in the block model
   * while it is the sum total of the number of functional blocks
   * of all the floorplans in the power dissipating layers of the 
   * grid model. 
   */
  if (model->type == BLOCK_MODEL)
    n = model->block->flp->n_units;
  else if (model->type == GRID_MODEL) {
      for(i=0; i < model->grid->n_layers; i++)
        if (model->grid->layers[i].has_power)
          n += model->grid->layers[i].flp->n_units;
  } else 
    fatal("unknown model type\n");

  /* read init file */
  if (do_transient && strcmp(model->config->init_file, NULLFILE)) {
      if (!model->config->dtm_used)	
        read_temp(model, init_temp, model->config->init_file, FALSE);
      else	
        read_temp(model, init_temp, model->config->init_file, TRUE);
  } else if (do_transient)	
    set_temp(model, init_temp, model->config->init_temp);

  //printf("*[Hotspot] end initialization!\n");
}

/*
 * implementation of calculateTemperature
 */
void
Hotspot::calculateTemperature(double *temp_rst)
{
	//printf("*[HotSpot] Here we begin calculate\n");
  int i, j, idx, base = 0, count = 0;

  char **names;

  double *vals;
  /* instantaneous temperature and power values	*/
  double *temp = NULL, *power;
  double total_power = 0.0;

  /* steady state temperature and power values	*/
  double *overall_power, *steady_temp;

  /* variables for natural convection iterations */
  int natural = 0; 
  double avg_sink_temp = 0;
  int natural_convergence = 0;
  double r_convec_old;

  /* trace file pointers	*/
  FILE *pin, *tout = NULL;

  lines = 0;

  /* if package model is used, run package model */
  if (((idx = get_str_index(table, size, "package_model_used")) >= 0) && !(table[idx].value==0)) {
      if (thermal_config.package_model_used) {
          avg_sink_temp = thermal_config.ambient + SMALL_FOR_CONVEC;
          natural = package_model(&thermal_config, table, size, avg_sink_temp);
          if (thermal_config.r_convec<R_CONVEC_LOW || thermal_config.r_convec>R_CONVEC_HIGH)
            printf("Warning: Heatsink convection resistance is not realistic, double-check your package settings...\n"); 
      }
  }

  /* allocate the temp and power arrays	*/
  /* using hotspot_vector to internally allocate any extra nodes needed	*/
  temp = hotspot_vector(model);
  power = hotspot_vector(model);
  steady_temp = hotspot_vector(model);
  overall_power = hotspot_vector(model);

	//printf("*[HotSpot] Here we finish configuration\n");

  /* set up initial instantaneous temperatures */
  
  /*
  if (first_call == TRUE) {
	first_call = FALSE;
    set_temp(model, temp, model->config->init_temp);
	printf("Set default initial temperature\n");
  } else {
    if (do_transient && !start_analysis) {
	  printf("Set last temperature as initial temperature\n");
	  copy_temp(model, temp, init_temp);
    } else {
	  printf("Set last steady temperature file as initial temperature\n");
	  if (!model->config->dtm_used)	
	    read_temp(model, temp, model->config->init_file, FALSE);
	  else	
	    read_temp(model, temp, model->config->init_file, TRUE);
	  start_analysis = FALSE;
    }
  }
  */
  /* Set init temperature without init file*/
  copy_temp(model, temp, init_temp);


  if(!(pin = fopen(global_config.p_infile, "r")))
    fatal("unable to open power trace input file\n");
  /*(
  if(do_transient && !(tout = fopen(global_config.t_outfile, "w")))
    fatal("unable to open temperature trace file for output\n");
	*/

  names = alloc_names(MAX_UNITS, STR_SIZE);
  if(read_names(pin, names) != n)
    fatal("no. of units in floorplan and trace file differ\n");

  /*
  if (do_transient)
    write_names(tout, names, n);
  if (do_transient)
    write_vals(tout, temp, n);
	*/

  /* read the instantaneous power trace	*/
  vals = dvector(MAX_UNITS);
  while ((num=read_vals(pin, vals)) != 0) {
      if(num != n) {
		//printf("%d:%d\n", num, n);
        fatal("invalid trace file format\n");
	  }

      /* permute the power numbers according to the floorplan order	*/
      if (model->type == BLOCK_MODEL)
        for(i=0; i < n; i++)
          power[get_blk_index(flp, names[i])] = vals[i];
      else
        for(i=0, base=0, count=0; i < model->grid->n_layers; i++) {
            if(model->grid->layers[i].has_power) {
                for(j=0; j < model->grid->layers[i].flp->n_units; j++) {
					//printf("%f\n", power[base+idx]);
                    idx = get_blk_index(model->grid->layers[i].flp, names[count+j]);
                    power[base+idx] = vals[count+j];
                }
                count += model->grid->layers[i].flp->n_units;
            }	
            base += model->grid->layers[i].flp->n_units;	
        }

      /* compute temperature	*/
      if (do_transient) {
          /* if natural convection is considered, update transient convection resistance first */
          if (natural) {
              avg_sink_temp = calc_sink_temp(model, temp);

			  //printf("avg_sink_temp: %.3f\n", avg_sink_temp);

              natural = package_model(model->config, table, size, avg_sink_temp);
              populate_R_model(model, flp);
          }
          /* for the grid model, only the first call to compute_temp
           * passes a non-null 'temp' array. if 'temp' is  NULL, 
           * compute_temp remembers it from the last non-null call. 
           * this is used to maintain the internal grid temperatures 
           * across multiple calls of compute_temp
           */
		  //printf("compute some tempratures\n");
          if (model->type == BLOCK_MODEL || lines == 0)
            compute_temp(model, power, temp, model->config->sampling_intvl);
          else
            compute_temp(model, power, NULL, model->config->sampling_intvl);

          /* permute back to the trace file order	*/
          if (model->type == BLOCK_MODEL)
            for(i=0; i < n; i++)
              vals[i] = temp[get_blk_index(flp, names[i])];
          else
            for(i=0, base=0, count=0; i < model->grid->n_layers; i++) {
                if(model->grid->layers[i].has_power) {
                    for(j=0; j < model->grid->layers[i].flp->n_units; j++) {
                        idx = get_blk_index(model->grid->layers[i].flp, names[count+j]);
                        vals[count+j] = temp[base+idx];
						/* Debug */
						//printf("[Hotspot] block name: %s, vals: %.5f\n", names[count+j], vals[count+j]);
                    }
                    count += model->grid->layers[i].flp->n_units;	
                }	
                base += model->grid->layers[i].flp->n_units;	
            }

          /* output instantaneous temperature trace	*/
          //write_vals(tout, vals, n);
      }		

      /* for computing average	*/
      if (model->type == BLOCK_MODEL)
        for(i=0; i < n; i++)
          overall_power[i] += power[i];
      else
        for(i=0, base=0; i < model->grid->n_layers; i++) {
            if(model->grid->layers[i].has_power)
              for(j=0; j < model->grid->layers[i].flp->n_units; j++)
                overall_power[base+j] += power[base+j];
            base += model->grid->layers[i].flp->n_units;	
        }

      lines++;
  }

  if(!lines)
    fatal("no power numbers in trace file\n");

  /* for computing average	*/
  if (model->type == BLOCK_MODEL)
    for(i=0; i < n; i++) {
        overall_power[i] /= lines;
        total_power += overall_power[i];
    }
  else
    for(i=0, base=0; i < model->grid->n_layers; i++) {
        if(model->grid->layers[i].has_power)
          for(j=0; j < model->grid->layers[i].flp->n_units; j++) {
              overall_power[base+j] /= lines;
              total_power += overall_power[base+j];
          }
        base += model->grid->layers[i].flp->n_units;	
    }

  /* natural convection r_convec iteration, for steady-state only */ 		
  natural_convergence = 0;
  if (natural) { /* natural convection is used */
      while (!natural_convergence) {
          r_convec_old = model->config->r_convec;
          /* steady state temperature	*/
          steady_state_temp(model, overall_power, steady_temp);
          avg_sink_temp = calc_sink_temp(model, steady_temp) + SMALL_FOR_CONVEC;
          natural = package_model(model->config, table, size, avg_sink_temp);
          populate_R_model(model, flp);
          if (avg_sink_temp > MAX_SINK_TEMP)
            fatal("too high power for a natural convection package -- possible thermal runaway\n");
          if (fabs(model->config->r_convec-r_convec_old)<NATURAL_CONVEC_TOL) 
            natural_convergence = 1;
      }
  }	else /* natural convection is not used, no need for iterations */
    /* steady state temperature	*/
    steady_state_temp(model, overall_power, steady_temp);

  /* for computing max temp */
  /*
  double steady_max = 0;
  if (model->type == BLOCK_MODEL)
    for(i=0; i < n; i++) {
		if (steady_temp[i] > steady_max)
			steady_max = steady_temp[i];
    }
  else
    for(i=0, base=0; i < model->grid->n_layers; i++) {
        if(model->grid->layers[i].has_power)
          for(j=0; j < model->grid->layers[i].flp->n_units; j++) {
			  if (steady_temp[base+j] > steady_max)
				  steady_max = steady_temp[base+j];
          }
        base += model->grid->layers[i].flp->n_units;	
    }
	*/

  /* print steady state results	*/
  /* dump steady state temperatures on to file if needed	*/
  if (strcmp(model->config->steady_file, NULLFILE)) {
	//printf("dump steady temperature to file: %s!\n", model->config->steady_file);
	dump_temp(model, steady_temp, model->config->steady_file);
	/* End Warm Up*/
	//if (steady_max > _start_analysis_threshold + 273.15)
	 // startAnalysis();
  }

  /* Get steady temperature for Sniper*/
  if (model->type == BLOCK_MODEL) {
    for(i=0; i < n; i++)
      temp_rst[i] = vals[i] - 273.15;
      //temp_rst[i] = steady_temp[i] - 273.15;
  } else {
    for(i=0, count=0; i < model->grid->n_layers; i++) {
        if(model->grid->layers[i].has_power) {
            for(j=0; j < model->grid->layers[i].flp->n_units; j++) {
				if (do_transient) {
					temp_rst[count+j] = vals[count+j] - 273.15;
				} 
            }
            count += model->grid->layers[i].flp->n_units;
        }	
    }
  }
  //printf("*[HotSpot] Here we end calculate\n");
  
  copy_temp(model, init_temp, temp);
  model->grid->last_temp = init_temp;

  /* cleanup	*/
  fclose(pin);
  /*
  if (do_transient)
    fclose(tout);
	*/
  free_dvector(temp);
  free_dvector(power);
  free_dvector(steady_temp);
  free_dvector(overall_power);
  free_names(names);
  free_dvector(vals);
  return;
}

void
Hotspot::endHotSpot()
{
  delete_RC_model(model);
  free_flp(flp, FALSE);
  free_dvector(init_temp);
}

void
Hotspot::startAnalysis()
{
  do_transient = TRUE;
  start_analysis = TRUE;
}

void
Hotspot::startWarmUp()
{
  do_transient = FALSE;
  start_analysis = FALSE;
}

/* 
 * main function - reads instantaneous power values (in W) from a trace
 * file (e.g. "gcc.ptrace") and outputs instantaneous temperature values (in C) to
 * a trace file("gcc.ttrace"). also outputs steady state temperature values
 * (including those of the internal nodes of the model) onto stdout. the
 * trace files are 2-d matrices with each column representing a functional
 * functional block and each row representing a time unit(sampling_intvl).
 * columns are tab-separated and each row is a separate line. the first
 * line contains the names of the functional blocks. the order in which
 * the columns are specified doesn't have to match that of the floorplan 
 * file.
 */
