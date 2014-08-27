// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/* For STRADS to support long type, int type is replaced with unsigned long int. 
   This code is based on mmio.cpp in http://math.nist.gov/MatrixMarket/mmio/c/mmio.c
*/

/* 
*   Matrix Market I/O library for ANSI C
*
*   See http://math.nist.gov/MatrixMarket for details.
*
*
*/

# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <ctype.h>
# include <time.h>
# include "mm_io.hpp"

/******************************************************************************/
int mm_is_valid ( MM_typecode matcode )
/******************************************************************************/
/*
  Purpose:
    MM_IS_VALID checks whether the MM header information is valid.
  Modified:
    31 October 2008
  Parameters:
    Input, MM_typecode MATCODE, the header information.
    Output, int MM_IS_VALID, is TRUE if the matrix code is valid.
*/
{
  if ( !mm_is_matrix ( matcode ) ) 
  {
    return 0;
  }
  if ( mm_is_dense ( matcode ) && mm_is_pattern ( matcode ) ) 
  {
    return 0;
  }
  if ( mm_is_real ( matcode ) && mm_is_hermitian ( matcode ) ) 
  {
    return 0;
  }
  if ( mm_is_pattern ( matcode ) && 
      ( mm_is_hermitian ( matcode ) || mm_is_skew ( matcode ) ) ) 
  {
    return 0;
  }
  return 1;
}

/******************************************************************************/
// MANDATORY
int mm_read_banner ( FILE *f, MM_typecode *matcode )
/******************************************************************************/
/*
  Purpose:
    MM_READ_BANNER reads the header line of an MM file.
  Modified:
    31 October 2008
    15 Oct 2012 to replace int with long unsigned int 
  Parameters:
    Input, FILE *F, a pointer to the input file.
    Output, MM_typecode *MATCODE, the header information.
*/
{
    char line[MM_MAX_LINE_LENGTH];
    char banner[MM_MAX_TOKEN_LENGTH];
    char mtx[MM_MAX_TOKEN_LENGTH]; 
    char crd[MM_MAX_TOKEN_LENGTH];
    char data_type[MM_MAX_TOKEN_LENGTH];
    char storage_scheme[MM_MAX_TOKEN_LENGTH];
    char *p;
    mm_clear_typecode(matcode);  
    if (fgets(line, MM_MAX_LINE_LENGTH, f) == NULL) 
        return MM_PREMATURE_EOF;
    // banner: MatrixMarket
    // mtx: matrix or vector 
    // format: coordinate or dense array 
    // field : real or int of value 
    // symmetry: general, symmetric, skew-symmetric ... 
    if (sscanf(line, "%s %s %s %s %s", banner, mtx, crd, data_type, 
        storage_scheme) != 5)
        return MM_PREMATURE_EOF;
    for (p=mtx; *p!='\0'; *p=tolower(*p),p++);  /* convert to lower case */
    for (p=crd; *p!='\0'; *p=tolower(*p),p++);  
    for (p=data_type; *p!='\0'; *p=tolower(*p),p++);
    for (p=storage_scheme; *p!='\0'; *p=tolower(*p),p++);
/* 
  check for banner 
*/
    if (strncmp(banner, MatrixMarketBanner, strlen(MatrixMarketBanner)) != 0)
        return MM_NO_HEADER;
/* 
  first field should be "mtx" 
*/
    if (strcmp(mtx, MM_MTX_STR) != 0)
        return  MM_UNSUPPORTED_TYPE;
    mm_set_matrix(matcode);
/* 
  second field describes whether this is a sparse matrix (in coordinate
  storgae) or a dense array 
*/
    if (strcmp(crd, MM_SPARSE_STR) == 0)
        mm_set_sparse(matcode);
    else
    if (strcmp(crd, MM_DENSE_STR) == 0)
            mm_set_dense(matcode);
    else
        return MM_UNSUPPORTED_TYPE;   
/*
  third field 
*/
    if (strcmp(data_type, MM_REAL_STR) == 0)
        mm_set_real(matcode);
    else
    if (strcmp(data_type, MM_COMPLEX_STR) == 0)
        mm_set_complex(matcode);
    else
    if (strcmp(data_type, MM_PATTERN_STR) == 0)
        mm_set_pattern(matcode);
    else
    if (strcmp(data_type, MM_INT_STR) == 0)
        mm_set_integer(matcode);
    else
        return MM_UNSUPPORTED_TYPE;    
/*
  fourth field 
*/
    if (strcmp(storage_scheme, MM_GENERAL_STR) == 0)
        mm_set_general(matcode);
    else
    if (strcmp(storage_scheme, MM_SYMM_STR) == 0)
        mm_set_symmetric(matcode);
    else
    if (strcmp(storage_scheme, MM_HERM_STR) == 0)
        mm_set_hermitian(matcode);
    else
    if (strcmp(storage_scheme, MM_SKEW_STR) == 0)
        mm_set_skew(matcode);
    else
        return MM_UNSUPPORTED_TYPE;        
    return 0;
}

/******************************************************************************/
// Mandatory for dense array format. 
int mm_read_mtx_array_size ( FILE *f, long unsigned int *M, long unsigned int *N )
/******************************************************************************/
/*
  Purpose:
    MM_READ_MTX_ARRAY_SIZE reads the size line of an MM array file.
  Modified:
    03 November 2008
  Parameters:
    Input, FILE *F, a pointer to the input file.
    Output, int *M, the number of rows, as read from the file.
    Output, int *N, the number of columns, as read from the file.
    Output, MM_READ_MTX_ARRAY_SIZE, an error flag.
    0, no error.
*/
{
  char line[MM_MAX_LINE_LENGTH];
  int num_items_read;
/* 
  set return null parameter values, in case we exit with errors 
*/
  *M = 0;
  *N = 0;
/* 
  now continue scanning until you reach the end-of-comments 
*/
  do 
  {
    if ( fgets ( line, MM_MAX_LINE_LENGTH, f ) == NULL ) 
    {
      return MM_PREMATURE_EOF;
    }
  } while ( line[0] == '%' );
/* 
  line[] is either blank or has M,N, nz 
*/
  if ( sscanf ( line, "%lu %lu", M, N ) == 2 )
  {
    return 0;
  }
  else
  {
    do
    { 
      num_items_read = fscanf ( f, "%lu %lu", M, N ); 
      if ( num_items_read == EOF ) 
      {
        return MM_PREMATURE_EOF;
      }
    }
    while ( num_items_read != 2 );
  }
  return 0;
}

/******************************************************************************/
// MANDATORY 
int mm_read_mtx_crd_size(FILE *f, long unsigned int *M, long unsigned int *N, long unsigned int *nz )
/******************************************************************************/
/*
  Purpose:
    MM_READ_MTX_CRD_SIZE reads the size line of an MM coordinate file.
  Modified:
    03 November 2008
  Parameters:
    Input, FILE *F, a pointer to the input file.
    Output, int *M, the number of rows, as read from the file.
    Output, int *N, the number of columns, as read from the file.
    Output, int *NZ, the number of nonzero values, as read from the file.
    Output, MM_READ_MTX_CRF_SIZE, an error flag.
    0, no error.
*/
{
  char line[MM_MAX_LINE_LENGTH];
  int num_items_read;
/* 
  set return null parameter values, in case we exit with errors 
*/
  *M = 0;
  *N = 0;
  *nz = 0;
/* 
  now continue scanning until you reach the end-of-comments 
*/
  do 
  {
    if (fgets(line,MM_MAX_LINE_LENGTH,f) == NULL) 
    {
      return MM_PREMATURE_EOF;
    }
  } while (line[0] == '%');
/* 
  line[] is either blank or has M,N, nz 
*/
  if (sscanf(line, "%lu %lu %lu", M, N, nz) == 3)
  {
    return 0;
  }
  else
  {
    do
    { 
      num_items_read = fscanf(f, "%lu %lu %lu", M, N, nz); 
      if (num_items_read == EOF) 
      {
        return MM_PREMATURE_EOF;
      }
    } while (num_items_read != 3);
  }
  return 0;
}
/******************************************************************************/

/******************************************************************************/
// mandatory
int mm_write_mtx_crd_size ( FILE *f, long unsigned int M, long unsigned int N, long unsigned int nz )
/******************************************************************************/
/*
  Purpose:
    MM_WRITE_MTX_CRD_SIZE writes the size line of an MM coordinate file. 
  Modified:
    31 October 2008
  Parameters:
    Input, FILE *F, a pointer to the output file.
*/
{
  if ( fprintf ( f, "%lu %lu %lu\n", M, N, nz ) != 3 )
  {
    return MM_COULD_NOT_WRITE_FILE;
  }
  else 
  {
    return 0;
  }
}

/******************************************************************************/
// Mandatory 
int mm_write_banner ( FILE *f, MM_typecode matcode )
/******************************************************************************/
/*
  Purpose:
    MM_WRITE_BANNER writes the header line of an MM file.
  Modified:
    31 October 2008
  Parameters:
    Input, FILE *F, a pointer to the output file.
*/
{
    char *str = mm_typecode_to_str(matcode);
    int ret_code;
    ret_code = fprintf(f, "%s %s\n", MatrixMarketBanner, str);
    free(str);
    if (ret_code !=2 )
        return MM_COULD_NOT_WRITE_FILE;
    else
        return 0;
}
/******************************************************************************/

int mm_write_mtx_array_size(FILE *f, long unsigned int M, long unsigned int N)
/******************************************************************************/
/*
  Purpose:
    MM_WRITE_MTX_ARRAY_SIZE writes the size line of an MM array file.
  Modified:
    31 October 2008
*/
{
    if (fprintf(f, "%lu %lu\n", M, N) != 2)
        return MM_COULD_NOT_WRITE_FILE;
    else 
        return 0;
}

/******************************************************************************/
void timestamp ( void )
/******************************************************************************/
/*
  Purpose:
    TIMESTAMP prints the current YMDHMS date as a time stamp.
  Example:
    31 May 2001 09:45:54 AM
  Licensing:
    This code is distributed under the GNU LGPL license. 
  Modified:
    24 September 2003
  Author:
    John Burkardt
  Parameters:
    None
*/
{
# define TIME_SIZE 40
  static char time_buffer[TIME_SIZE];
  const struct tm *tm;

  time_t now;
  now = time ( NULL );
  tm = localtime ( &now );
  strftime ( time_buffer, TIME_SIZE, "%d %B %Y %I:%M:%S %p", tm );
  printf ( "%s\n", time_buffer );
  return;
# undef TIME_SIZE
}

char *mm_strdup ( const char *s )
/******************************************************************************/
/*
  Purpose:
    MM_STRDUP creates a new copy of a string.
  Discussion:
    This is a common routine, but not part of ANSI C, so it is included here.  
    It is used by mm_typecode_to_str().
  Modified:
    31 October 2008
*/
{
  int len = strlen ( s );
  char *s2 = (char *) malloc((len+1)*sizeof(char));
  return strcpy(s2, s);
}
/******************************************************************************/

char *mm_typecode_to_str ( MM_typecode matcode )
/******************************************************************************/
/*
  Purpose:
    MM_TYPECODE_TO_STR converts the internal typecode to an MM header string.
  Modified:
    31 October 2008
*/
{
    char buffer[MM_MAX_LINE_LENGTH];
    char *types[4];
	char *mm_strdup(const char *);
	//int error =0;
/* 
  check for MTX type 
*/
    if (mm_is_matrix(matcode)) 
      types[0] = MM_MTX_STR;
    //    else
    //    error=1;
/* 
  check for CRD or ARR matrix 
*/
    if (mm_is_sparse(matcode))
        types[1] = MM_SPARSE_STR;
    else
    if (mm_is_dense(matcode))
        types[1] = MM_DENSE_STR;
    else
        return NULL;
/* 
  check for element data type 
*/
    if (mm_is_real(matcode))
        types[2] = MM_REAL_STR;
    else
    if (mm_is_complex(matcode))
        types[2] = MM_COMPLEX_STR;
    else
    if (mm_is_pattern(matcode))
        types[2] = MM_PATTERN_STR;
    else
    if (mm_is_integer(matcode))
        types[2] = MM_INT_STR;
    else
        return NULL;
/* 
  check for symmetry type 
*/
    if (mm_is_general(matcode))
        types[3] = MM_GENERAL_STR;
    else
    if (mm_is_symmetric(matcode))
        types[3] = MM_SYMM_STR;
    else 
    if (mm_is_hermitian(matcode))
        types[3] = MM_HERM_STR;
    else 
    if (mm_is_skew(matcode))
        types[3] = MM_SKEW_STR;
    else
        return NULL;
    sprintf(buffer,"%s %s %s %s", types[0], types[1], types[2], types[3]);
    return mm_strdup(buffer);
}

































































#if 0 
/******************************************************************************/

int mm_read_mtx_crd(char *fname, int *M, int *N, int *nz, int **I, int **J, 
        double **val, MM_typecode *matcode)

/******************************************************************************/
/*
  Purpose:

    MM_READ_MTX_CRD reads the values in an MM coordinate file.

  Discussion:

    This function allocates the storage for the arrays.

  Modified:

    31 October 2008

  Parameters:

*/
/*
    mm_read_mtx_crd()  fills M, N, nz, array of values, and return
                        type code, e.g. 'MCRS'

                        if matrix is complex, values[] is of size 2*nz,
                            (nz pairs of real/imaginary values)
*/
{
    int ret_code;
    FILE *f;

    if (strcmp(fname, "stdin") == 0) f=stdin;
    else
    if ((f = fopen(fname, "r")) == NULL)
        return MM_COULD_NOT_READ_FILE;


    if ((ret_code = mm_read_banner(f, matcode)) != 0)
        return ret_code;

    if (!(mm_is_valid(*matcode) && mm_is_sparse(*matcode) && 
            mm_is_matrix(*matcode)))
        return MM_UNSUPPORTED_TYPE;

    if ((ret_code = mm_read_mtx_crd_size(f, M, N, nz)) != 0)
        return ret_code;


    *I = (int *)  malloc(*nz * sizeof(int));
    *J = (int *)  malloc(*nz * sizeof(int));
    *val = NULL;

    if (mm_is_complex(*matcode))
    {
        *val = (double *) malloc(*nz * 2 * sizeof(double));
        ret_code = mm_read_mtx_crd_data(f, *M, *N, *nz, *I, *J, *val, 
                *matcode);
        if (ret_code != 0) return ret_code;
    }
    else if (mm_is_real(*matcode))
    {
        *val = (double *) malloc(*nz * sizeof(double));
        ret_code = mm_read_mtx_crd_data(f, *M, *N, *nz, *I, *J, *val, 
                *matcode);
        if (ret_code != 0) return ret_code;
    }

    else if (mm_is_pattern(*matcode))
    {
        ret_code = mm_read_mtx_crd_data(f, *M, *N, *nz, *I, *J, *val, 
                *matcode);
        if (ret_code != 0) return ret_code;
    }

    if (f != stdin) fclose(f);
    return 0;
}
/******************************************************************************/

int mm_read_mtx_crd_data(FILE *f, int M, int N, int nz, int I[], int J[],
        double val[], MM_typecode matcode)

/******************************************************************************/
/*
  Purpose:

    MM_READ_MTX_CRD_DATA reads the values in an MM coordinate file.

  Discussion:

    This function assumes the array storage has already been allocated.

  Modified:

    31 October 2008

  Parameters:

    Input, FILE *F, a pointer to the input file.
*/
{
    int i;
    if (mm_is_complex(matcode))
    {
        for (i=0; i<nz; i++)
            if (fscanf(f, "%d %d %lg %lg", &I[i], &J[i], &val[2*i], &val[2*i+1])
                != 4) return MM_PREMATURE_EOF;
    }
    else if (mm_is_real(matcode))
    {
        for (i=0; i<nz; i++)
        {
            if (fscanf(f, "%d %d %lg\n", &I[i], &J[i], &val[i])
                != 3) return MM_PREMATURE_EOF;

        }
    }

    else if (mm_is_pattern(matcode))
    {
        for (i=0; i<nz; i++)
            if (fscanf(f, "%d %d", &I[i], &J[i])
                != 2) return MM_PREMATURE_EOF;
    }
    else
        return MM_UNSUPPORTED_TYPE;

    return 0;
        
}
/******************************************************************************/

int mm_read_mtx_crd_entry(FILE *f, int *I, int *J,
        double *real, double *imag, MM_typecode matcode)

/******************************************************************************/
/*
  Purpose:

    MM_READ_MTX_CRD_ENTRY ???

  Modified:

    31 October 2008

  Parameters:

    Input, FILE *F, a pointer to the input file.
*/
{
    if (mm_is_complex(matcode))
    {
            if (fscanf(f, "%d %d %lg %lg", I, J, real, imag)
                != 4) return MM_PREMATURE_EOF;
    }
    else if (mm_is_real(matcode))
    {
            if (fscanf(f, "%d %d %lg\n", I, J, real)
                != 3) return MM_PREMATURE_EOF;

    }

    else if (mm_is_pattern(matcode))
    {
            if (fscanf(f, "%d %d", I, J) != 2) return MM_PREMATURE_EOF;
    }
    else
        return MM_UNSUPPORTED_TYPE;

    return 0;
        
}

int mm_read_unsymmetric_sparse(const char *fname, int *M_, int *N_, int *nz_,
                double **val_, int **I_, int **J_)

/******************************************************************************/
/*
  Purpose:

    MM_READ_UNSYMMETRIC_SPARSE ???

  Modified:

    31 October 2008
*/
{
    FILE *f;
    MM_typecode matcode;
    int M, N, nz;
    int i;
    double *val;
    int *I, *J;
 
    if ((f = fopen(fname, "r")) == NULL)
            return -1;
 
 
    if (mm_read_banner(f, &matcode) != 0)
    {
        printf("mm_read_unsymetric: Could not process Matrix Market banner ");
        printf(" in file [%s]\n", fname);
        return -1;
    }
 
 
 
    if ( !(mm_is_real(matcode) && mm_is_matrix(matcode) &&
            mm_is_sparse(matcode)))
    {
        fprintf(stderr, "Sorry, this application does not support ");
        fprintf(stderr, "Market Market type: [%s]\n",
                mm_typecode_to_str(matcode));
        return -1;
    }
 
/* 
  find out size of sparse matrix: M, N, nz .... 
*/
 
    if (mm_read_mtx_crd_size(f, &M, &N, &nz) !=0)
    {
        fprintf(stderr, "read_unsymmetric_sparse(): could not parse matrix size.\n");
        return -1;
    }
 
    *M_ = M;
    *N_ = N;
    *nz_ = nz;
/* 
  reseve memory for matrices 
*/ 
    I = (int *) malloc(nz * sizeof(int));
    J = (int *) malloc(nz * sizeof(int));
    val = (double *) malloc(nz * sizeof(double));
 
    *val_ = val;
    *I_ = I;
    *J_ = J;
/* 
  NOTE: when reading in doubles, ANSI C requires the use of the "l"
  specifier as in "%lg", "%lf", "%le", otherwise errors will occur
  (ANSI C X3.159-1989, Sec. 4.9.6.2, p. 136 lines 13-15) 
*/
 
    for (i=0; i<nz; i++)
    {
        fscanf(f, "%d %d %lg\n", &I[i], &J[i], &val[i]);
/*
  Adjust from 1-based to 0-based.
*/
        I[i]--;
        J[i]--;
    }
    fclose(f);
 
    return 0;
}
/******************************************************************************/


int mm_write_mtx_crd(char fname[], int M, int N, int nz, int I[], int J[],
        double val[], MM_typecode matcode)

/******************************************************************************/
/*
  Purpose:

    MM_WRITE_MTX_CRD writes an MM coordinate file.

  Modified:

    31 October 2008
*/
{
    FILE *f;
    int i;

    if (strcmp(fname, "stdout") == 0) 
        f = stdout;
    else
    if ((f = fopen(fname, "w")) == NULL)
        return MM_COULD_NOT_WRITE_FILE;
/*
  print banner followed by typecode.
*/
    fprintf(f, "%s ", MatrixMarketBanner);
    fprintf(f, "%s\n", mm_typecode_to_str(matcode));

/*
  print matrix sizes and nonzeros.
*/
    fprintf(f, "%d %d %d\n", M, N, nz);
/* 
  print values.
*/
    if (mm_is_pattern(matcode))
        for (i=0; i<nz; i++)
            fprintf(f, "%d %d\n", I[i], J[i]);
    else
    if (mm_is_real(matcode))
        for (i=0; i<nz; i++)
            fprintf(f, "%d %d %20.16g\n", I[i], J[i], val[i]);
    else
    if (mm_is_complex(matcode))
        for (i=0; i<nz; i++)
            fprintf(f, "%d %d %20.16g %20.16g\n", I[i], J[i], val[2*i], 
                        val[2*i+1]);
    else
    {
        if (f != stdout) fclose(f);
        return MM_UNSUPPORTED_TYPE;
    }

  if ( f !=stdout ) 
  {
    fclose(f);
  }
  return 0;
}

#endif 

