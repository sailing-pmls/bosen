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
   This code is based on mmio.hpp in http://math.nist.gov/MatrixMarket/mmio/c/mmio.c
*/


/* 
*   Matrix Market I/O library for ANSI C
*
*   See http://math.nist.gov/MatrixMarket for details.
*
*
*/

# ifndef MM_IO_H
# define MM_IO_H

# define MM_MAX_LINE_LENGTH 1025
# define MatrixMarketBanner "%%MatrixMarket"
# define MM_MAX_TOKEN_LENGTH 64

typedef char MM_typecode[4];

int mm_is_valid ( MM_typecode matcode );
int mm_read_banner ( FILE *f, MM_typecode *matcode );

int mm_read_mtx_array_size ( FILE *f, long unsigned int *M, long unsigned int *N );
int mm_read_mtx_crd_size ( FILE *f, long unsigned int *M, long unsigned int *N, long unsigned int *nz );
int mm_write_mtx_crd_size ( FILE *f, long unsigned int M, long unsigned int N, long unsigned int nz );
int mm_write_banner ( FILE *f, MM_typecode matcode );
int mm_write_mtx_array_size ( FILE *f, long unsigned int M, long unsigned int N );
void timestamp ( void );

char *mm_strdup ( const char *s );
char *mm_typecode_to_str ( MM_typecode matcode );


#if 0 
int mm_read_mtx_crd(char *fname, int *M, int *N, int *nz, int **I, int **J, 
  double **val, MM_typecode *matcode);
int mm_read_mtx_crd_data ( FILE *f, int M, int N, int nz, int I[], int J[],
  double val[], MM_typecode matcode );
int mm_read_mtx_crd_entry ( FILE *f, int *I, int *J, double *real, double *img,
  MM_typecode matcode );
int mm_read_unsymmetric_sparse ( const char *fname, int *M_, int *N_, int *nz_,
  double **val_, int **I_, int **J_ );
int mm_write_mtx_crd ( char fname[], int M, int N, int nz, int I[], int J[],
  double val[], MM_typecode matcode );
#endif


/********************* MM_typecode query functions ***************************/
# define mm_is_matrix(typecode)	((typecode)[0]=='M')

# define mm_is_sparse(typecode)	((typecode)[1]=='C')
# define mm_is_coordinate(typecode)((typecode)[1]=='C')
# define mm_is_dense(typecode)	((typecode)[1]=='A')
# define mm_is_array(typecode)	((typecode)[1]=='A')

# define mm_is_complex(typecode)	((typecode)[2]=='C')
# define mm_is_real(typecode)		((typecode)[2]=='R')
# define mm_is_pattern(typecode)	((typecode)[2]=='P')
# define mm_is_integer(typecode) ((typecode)[2]=='I')

# define mm_is_symmetric(typecode)((typecode)[3]=='S')
# define mm_is_general(typecode)	((typecode)[3]=='G')
# define mm_is_skew(typecode)	((typecode)[3]=='K')
# define mm_is_hermitian(typecode)((typecode)[3]=='H')


/********************* MM_typecode modify functions ***************************/

# define mm_set_matrix(typecode)	((*typecode)[0]='M')
# define mm_set_coordinate(typecode)	((*typecode)[1]='C')
# define mm_set_array(typecode)	((*typecode)[1]='A')
# define mm_set_dense(typecode)	mm_set_array(typecode)
# define mm_set_sparse(typecode)	mm_set_coordinate(typecode)

# define mm_set_complex(typecode)((*typecode)[2]='C')
# define mm_set_real(typecode)	((*typecode)[2]='R')
# define mm_set_pattern(typecode)((*typecode)[2]='P')
# define mm_set_integer(typecode)((*typecode)[2]='I')


# define mm_set_symmetric(typecode)((*typecode)[3]='S')
# define mm_set_general(typecode)((*typecode)[3]='G')
# define mm_set_skew(typecode)	((*typecode)[3]='K')
# define mm_set_hermitian(typecode)((*typecode)[3]='H')

# define mm_clear_typecode(typecode) ((*typecode)[0]=(*typecode)[1]= \
									(*typecode)[2]=' ',(*typecode)[3]='G')

# define mm_initialize_typecode(typecode) mm_clear_typecode(typecode)


/********************* Matrix Market error codes ***************************/


# define MM_COULD_NOT_READ_FILE	11
# define MM_PREMATURE_EOF		12
# define MM_NOT_MTX				13
# define MM_NO_HEADER			14
# define MM_UNSUPPORTED_TYPE		15
# define MM_LINE_TOO_LONG		16
# define MM_COULD_NOT_WRITE_FILE	17


/******************** Matrix Market internal definitions ********************

   MM_matrix_typecode: 4-character sequence

				    ojbect 		sparse/   	data        storage 
						  		dense     	type        scheme

   string position:	 [0]        [1]			[2]         [3]

   Matrix typecode:  M(atrix)  C(oord)		R(eal)   	G(eneral)
						        A(array)	C(omplex)   H(ermitian)
											P(attern)   S(ymmetric)
								    		I(nteger)	K(kew)

 ***********************************************************************/



// IF not work, remove char * to each constanct string 
# define MM_MTX_STR	(char*)"matrix"
# define MM_ARRAY_STR	(char*)"array"
# define MM_DENSE_STR	(char*)"array"
# define MM_COORDINATE_STR (char*)"coordinate" 
# define MM_SPARSE_STR	(char*)"coordinate"
# define MM_COMPLEX_STR	(char*)"complex"
# define MM_REAL_STR	(char*)"real"
# define MM_INT_STR	(char*)"integer"
# define MM_GENERAL_STR (char*)"general"
# define MM_SYMM_STR	(char*)"symmetric"
# define MM_HERM_STR	(char*)"hermitian"
# define MM_SKEW_STR	(char*)"skew-symmetric"
# define MM_PATTERN_STR (char*)"pattern"

# endif
