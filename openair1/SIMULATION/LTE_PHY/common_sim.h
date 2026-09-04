/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PHY/defs_common.h"

static inline double time_stats_value_us(time_stats_t *ptr, oai_cputime_t (*getter)(time_stats_sorted_list_t *))
{
  if (ptr == NULL || !is_enabled_time_stats_sorted_list(&ptr->time_stats_sorted_list))
    return 0;
  oai_cputime_t value = getter(&ptr->time_stats_sorted_list);
  return value >= 0 ? value / 1000.0 : 0;
}

void sumUpStats(time_stats_t * res, time_stats_t * src, int lastActive)
{
  reset_meas(res);
  for (int i = 0; i < RX_NB_TH; i++) {
    merge_meas(res, &src[i]);
  }
  res->p_time=src[lastActive].p_time;
}

void sumUpStatsSlot(time_stats_t *res, time_stats_t src[RX_NB_TH][2], int lastActive)
{
  reset_meas(res);
  for (int i = 0; i < RX_NB_TH; i++) {
    merge_meas(res, &src[i][1]);
    merge_meas(res, &src[i][2]);
  }
  int last=src[lastActive][0].in < src[lastActive][1].in? 1 : 0 ;
  res->p_time=src[lastActive][last].p_time;
}

void printDistribution(time_stats_t *ptr, char *txt)
{
  printf("%-43s %6.2f us (%d trials)\n",
         txt,
         ptr->trials ? (double)ptr->diff / ptr->trials / 1000.0 : 0,
         ptr->trials);

  printf(" Statistics std=%.2f, min=%.2f, q1=%.2f, median=%.2f, q3=%.2f, max=%.2f µs (on %d trials)\n",
         ptr->trials ? get_std_dev(ptr) : 0,
         time_stats_value_us(ptr, get_min),
         time_stats_value_us(ptr, get_q1),
         time_stats_value_us(ptr, get_median),
         time_stats_value_us(ptr, get_q3),
         ptr->max / 1000.0,
         ptr->trials);
}

void printDistributionCsv(FILE *fd, time_stats_t *ptr, char *name)
{
  fprintf(fd,
          "%s;%f;%f;%f;%f;%f;%f;",
          name,
          get_std_dev(ptr),
          time_stats_value_us(ptr, get_min),
          time_stats_value_us(ptr, get_q1),
          time_stats_value_us(ptr, get_median),
          time_stats_value_us(ptr, get_q3),
          ptr->max / 1000.0);
}

void printDistributionDroppedCsv(FILE *fd, time_stats_t *ptr, int n_dropped, char *name)
{
  fprintf(fd,
          "%s;%f;%f;%f;%f;%f;%f;%d;",
          name,
          get_std_dev(ptr),
          time_stats_value_us(ptr, get_min),
          time_stats_value_us(ptr, get_q1),
          time_stats_value_us(ptr, get_median),
          time_stats_value_us(ptr, get_q3),
          ptr->max / 1000.0,
          n_dropped);
}

void printStatIndent(time_stats_t *ptr, char *txt)
{
  printf("|__ %-38s %6.2f us (%3d trials)\t\t(%6.2f total [ms])\n",
         txt,
         ptr->trials?ptr->diff/ptr->trials/1000.0:0,
         ptr->trials,
         ptr->trials?ptr->diff/1000.0:0);
}

void printStatIndent2(time_stats_t *ptr, char *txt)
{
  printf("    |__ %-34s %6.2f us (%3d trials)\t\t(%6.2f total [ms])\n",
         txt,
         ptr->trials?ptr->diff/ptr->trials/1000.0:0,
         ptr->trials,
         ptr->trials?ptr->diff/1000.0:0);
}

void printStatIndent3(time_stats_t *ptr, char *txt)
{
  printf("        |__ %-30s %6.2f us (%3d trials)\n",
         txt,
         ptr->trials?ptr->diff/ptr->trials/1000.0:0,
	 ptr->trials);
}


void logDistribution(FILE* fd, time_stats_t *ptr, int dropped)
{
  fprintf(fd, "%f;%f;%f;%f;%f;%f;%d;",
          ptr->trials ? get_std_dev(ptr) : 0,
          ptr->max / 1000.0,
          time_stats_value_us(ptr, get_min),
          time_stats_value_us(ptr, get_median),
          time_stats_value_us(ptr, get_q1),
          time_stats_value_us(ptr, get_q3),
          dropped);
}

struct option * parse_oai_options(paramdef_t *options) {
  int l;

  for(l=0; options[l].optname[0]!=0; l++) {};

  struct option *long_options=calloc(sizeof(struct option),l);

  for(int i=0; options[i].optname[0]!=0; i++) {
    long_options[i].name=options[i].optname;
    long_options[i].has_arg=options[i].paramflags==PARAMFLAG_BOOL?no_argument:required_argument;

    if ( options[i].voidptr)
      switch (options[i].type) {
      case TYPE_INT:
	*options[i].iptr=options[i].defintval;
	break;

      case TYPE_DOUBLE:
	*options[i].dblptr=options[i].defdblval;
	break;

      case TYPE_UINT8:
	*options[i].u8ptr=options[i].defintval;
	break;

      case TYPE_UINT16:
	*options[i].u16ptr=options[i].defintval;
	break;

      default:
	printf("not parsed type for default value %s\n", options[i].optname );
	exit(1);
      }

    continue;
  };
  return long_options;
}

void display_options_values(paramdef_t *options, int verbose) {
  for(paramdef_t * ptr=options; ptr->optname[0]!=0; ptr++) {
    char varText[256]={0};

    if (ptr->voidptr != NULL) {
      if ( (ptr->paramflags & PARAMFLAG_BOOL) )
        strcpy(varText, *(bool *)ptr->iptr ? "True": "False" );
      else  switch (ptr->type) {
  	 case TYPE_INT:
	   sprintf(varText,"%d",*ptr->iptr);
	  break;
	  
          case TYPE_DOUBLE:
            sprintf(varText,"%.2f",*ptr->dblptr);
            break;

	case TYPE_UINT8:
	  sprintf(varText,"%d",(int)*ptr->u8ptr);
	  break;

	case TYPE_UINT16:
	  sprintf(varText,"%d",(int)*ptr->u16ptr);
	  break;

	default:
	  strcpy(varText,"Need specific display");
	  printf("not decoded type\n");
	  exit(1);
        }
    }

    printf("--%-20s set to %s\n",ptr->optname, varText);
    if (verbose) printf("%s\n",ptr->helpstr);
  }
}
