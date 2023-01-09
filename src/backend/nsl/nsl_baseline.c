/*
	File                 : nsl_baseline.c
	Project              : LabPlot
	Description          : NSL baseline detection and subtraction functions
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2016-2023 Stefan Gerlach <stefan.gerlach@uni.kn>
	SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "nsl_baseline.h"
#include "nsl_stats.h"

#include "gsl/gsl_fit.h"
#include "gsl/gsl_linalg.h"
#include "gsl/gsl_sort.h"
#include "gsl/gsl_spblas.h"
#include "gsl/gsl_spmatrix.h"
#include "gsl/gsl_statistics_double.h"

#include <string.h> // memcpy

void nsl_baseline_remove_minimum(double* data, const size_t n) {
	const double min = nsl_stats_minimum(data, n, NULL);

	for (size_t i = 0; i < n; i++)
		data[i] -= min;
}

void nsl_baseline_remove_maximum(double* data, const size_t n) {
	const double max = nsl_stats_maximum(data, n, NULL);

	for (size_t i = 0; i < n; i++)
		data[i] -= max;
}

void nsl_baseline_remove_mean(double* data, const size_t n) {
	const double mean = gsl_stats_mean(data, 1, n);

	for (size_t i = 0; i < n; i++)
		data[i] -= mean;
}

void nsl_baseline_remove_median(double* data, const size_t n) {
	// copy data
	double* tmp_data = (double*)malloc(n * sizeof(double));
	if (!tmp_data)
		return;
	memcpy(tmp_data, data, n * sizeof(double));

	const double median = gsl_stats_median(tmp_data, 1, n); // rearranges tmp_data
	// printf("MEDIAN = %g\n", median);

	for (size_t i = 0; i < n; i++)
		data[i] -= median;

	free(tmp_data);
}

/* do a linear interpolation using first and last point and substract that */
int nsl_baseline_remove_endpoints(double* xdata, double* ydata, const size_t n) {
	// not possible
	if (xdata[0] == xdata[n - 1])
		return -1;

	for (size_t i = 0; i < n; i++) {
		// y = y1 + (x-x1)*(y2-y1)/(x2-x1)
		const double y = ydata[0] + (xdata[i] - xdata[0]) * (ydata[n - 1] - ydata[0]) / (xdata[n - 1] - xdata[0]);
		ydata[i] -= y;
	}

	return 0;
}

/* do a linear regression and substract that */
int nsl_baseline_remove_linreg(double* xdata, double* ydata, const size_t n) {
	double c0, c1, cov00, cov01, cov11, chisq;
	gsl_fit_linear(xdata, 1, ydata, 1, n, &c0, &c1, &cov00, &cov01, &cov11, &chisq);

	for (size_t i = 0; i < n; i++) {
		double y, y_err;
		gsl_fit_linear_est(xdata[i], c0, c1, cov00, cov01, cov11, &y, &y_err);
		ydata[i] -= y;
	}

	return 0;
}

void show_matrix(gsl_spmatrix* M, size_t n, size_t m, char name) {
	printf("%c:\n", name);
	for (size_t i = 0; i < n; ++i) {
		for (size_t j = 0; j < m; ++j)
			printf("%g ", gsl_spmatrix_get(M, i, j));
		puts("\n");
	}
}
void show_vector(gsl_vector* v, size_t n, char name) {
	printf("%c:\n", name);
	for (size_t i = 0; i < n; ++i)
		printf("%g ", gsl_vector_get(v, i));
	puts("\n");
}

/* see https://pubs.rsc.org/en/content/articlelanding/2015/AN/C4AN01061B#!divAbstract */
int nsl_baseline_remove_arpls(double* data, const size_t n, double p, double lambda, int niter) {
	if (p == 0)
		p = 0.001;
	if (lambda == 0)
		lambda = 1.e4;
	if (niter == 0)
		niter = 10;

	gsl_spmatrix* D = gsl_spmatrix_alloc(n, n - 2);
	for (size_t i = 0; i < n; ++i) {
		for (size_t j = 0; j < n - 2; ++j) {
			if (i == j)
				gsl_spmatrix_set(D, i, j, 1.);
			if (i == j + 1)
				gsl_spmatrix_set(D, i, j, -2.);
			if (i == j + 2)
				gsl_spmatrix_set(D, i, j, 1.);
		}
	}
	// show_matrix(D, n, n-2, 'D');

	// H = lambda * D.dot(D.T)
	gsl_spmatrix* DT = gsl_spmatrix_alloc(n - 2, n);
	gsl_spmatrix_transpose_memcpy(DT, D);

	gsl_spmatrix* H = gsl_spmatrix_alloc_nzmax(n, n, 5 * n, GSL_SPMATRIX_CSC);
	// requires compressed format
	gsl_spmatrix* DD = gsl_spmatrix_ccs(D);
	gsl_spmatrix* DDT = gsl_spmatrix_ccs(DT);
	gsl_spblas_dgemm(lambda, DD, DDT, H);
	// show_matrix(H, n, n, 'H');
	gsl_spmatrix_free(D);
	gsl_spmatrix_free(DT);

	// weights
	gsl_vector* w = gsl_vector_alloc(n);
	gsl_spmatrix* W = gsl_spmatrix_alloc_nzmax(n, n, n, GSL_SPMATRIX_COO);
	for (size_t i = 0; i < n; ++i) {
		gsl_vector_set(w, i, 1.);
		gsl_spmatrix_set(W, i, i, 1.);
	}
	// show_matrix(W, n, n, 'W');
	gsl_spmatrix* WW = gsl_spmatrix_ccs(W);

	// loop
	double crit = 1.;
	int count = 0;
	gsl_vector* d = gsl_vector_alloc(n); // data
	for (size_t i = 0; i < n; i++)
		gsl_vector_set(d, i, data[i]);
	gsl_vector* z = gsl_vector_alloc(n); // solution
	/* initial guess z */
	for (size_t i = 0; i < n; i++)
		gsl_vector_set(z, i, data[i]);
	gsl_vector* diff = gsl_vector_alloc(n); // diff
	gsl_vector* w_new = gsl_vector_alloc(n);

	gsl_spmatrix* A = gsl_spmatrix_alloc_nzmax(n, n, 5 * n, GSL_SPMATRIX_CSC);
	gsl_matrix* AA = gsl_matrix_alloc(n, n);
	gsl_vector* b = gsl_vector_alloc(n);
	gsl_permutation* per = gsl_permutation_alloc(n);
	int signum;
	while (crit > p) {
		printf("iteration %d\n", count);
		// solve (W+H)z = W*data

		// Az=b
		gsl_spmatrix_add(A, WW, H);
		// show_matrix(A, n, n, 'A');

		gsl_spblas_dgemv(CblasNoTrans, 1., W, d, 0., b);
		// show_vector(b, n, 'b');

		gsl_spmatrix_sp2d(AA, A);

		// LU
		gsl_linalg_LU_decomp(AA, per, &signum);
		gsl_linalg_LU_solve(AA, per, b, z);

		// LDLT: slower
		// gsl_linalg_ldlt_decomp(AA);
		// gsl_linalg_ldlt_solve(AA, b, z);
		//  Householder: slowest
		// gsl_linalg_HH_solve(AA, b, z);
		//  sparse iterative solver: is not working

		// show_vector(z, n, 'z');

		for (size_t i = 0; i < n; ++i)
			gsl_vector_set(diff, i, gsl_vector_get(d, i) - gsl_vector_get(z, i));
		// show_vector(diff, n, 'D');

		double m = 0.;
		int num = 0;
		for (size_t i = 0; i < n; ++i) {
			double v = gsl_vector_get(diff, i);
			if (v < 0) {
				m += v;
				num++;
			}
		}
		if (num > 0)
			m /= num;
		// printf("m = %g\n", m);
		double s = 0.;
		for (size_t i = 0; i < n; ++i) {
			double v = gsl_vector_get(diff, i);
			if (v < 0)
				s += gsl_pow_2(v - m);
		}
		if (num > 0)
			s /= num;
		s = sqrt(s);
		// printf("s = %g\n", s);

		// w_new = 1 / (1 + np.exp(2 * (d - (2*s - m))/s))
		for (size_t i = 0; i < n; ++i)
			gsl_vector_set(w_new, i, 1. / (1. + exp(2. * (gsl_vector_get(diff, i) - (2. * s - m)) / s)));
		// show_vector(w_new, n, 'n');

		// crit = norm(w_new - w) / norm(w)
		double norm = 0.;
		for (size_t i = 0; i < n; ++i) {
			norm += gsl_pow_2(gsl_vector_get(w_new, i) - gsl_vector_get(w, i));
		}
		norm = sqrt(norm);
		crit = norm / gsl_blas_dnrm2(w);
		// printf("norm (w_new - w) = %g\n", norm);
		// printf("norm (w) = %g\n", gsl_blas_dnrm2(w));
		printf("crit = %g\n", crit);

		// w = w_new
		gsl_vector_memcpy(w, w_new);
		// W.setdiag(w)
		for (size_t i = 0; i < n; ++i)
			gsl_spmatrix_set(W, i, i, gsl_vector_get(w_new, i));
		// show_matrix(W, n, n, 'W');
		WW = gsl_spmatrix_ccs(W);

		count++;

		if (count > niter) {
			puts("Maximum number of iterations reached");
			break;
		}
	}

	for (size_t i = 0; i < n; ++i) {
		// printf("%.15g ", data[i] - gsl_vector_get(z, i));
		data[i] -= gsl_vector_get(z, i);
	}
	// puts("\n");

	// gsl_splinalg_itersolve_free(work);
	gsl_matrix_free(AA);
	gsl_spmatrix_free(A);
	gsl_vector_free(b);
	gsl_spmatrix_free(H);
	gsl_vector_free(w);
	gsl_spmatrix_free(W);
	gsl_vector_free(w_new);
	gsl_vector_free(diff);
	gsl_vector_free(z);
	gsl_vector_free(d);

	return 0;
}
