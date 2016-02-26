#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include <Python.h>
#include <numpy/arrayobject.h>

#include "ral_nlls.h"

struct callback_data {
   PyObject* f;
   PyObject* J;
   PyObject* Hf;
   PyObject* params;
};

static
int eval_f(int n, int m, const void *params, const double *x, double *f) {
   // Recover our datatype
   const struct callback_data *data = (struct callback_data*) params;

   // Copy x into Python array
   npy_intp xdim[] = {n};
   PyArrayObject* xpy = (PyArrayObject*) PyArray_SimpleNew(1, xdim, NPY_DOUBLE);
   double* xval = (double*) PyArray_DATA(xpy);
   for(int i=0; i<n; ++i)
      xval[i] = x[i];

   // Call routine
   PyObject *arglist;
   if(data->params)  arglist = Py_BuildValue("(OO)", xpy, data->params);
   else              arglist = Py_BuildValue("(O)", xpy);
   PyObject *result = PyObject_CallObject(data->f, arglist);
   Py_DECREF(arglist);
   Py_DECREF(xpy);
   if(!result) return -1;

   // Extract result
   PyArrayObject* farray = (PyArrayObject*) PyArray_FROM_OTF(result, NPY_FLOAT64, NPY_ARRAY_IN_ARRAY);
   if(farray == NULL) {
      PyErr_SetString(PyExc_RuntimeError, "Error extracting array from f call");
      Py_DECREF(result);
      return -1;
   }
   if(PyArray_NDIM(farray) != 1) {
      PyErr_SetString(PyExc_RuntimeError, "f() must return rank-1 array");
      Py_DECREF(farray);
      Py_DECREF(result);
      return -2;
   }
   const double *fval = (double*) PyArray_DATA(farray);
   for(int i=0; i<m; ++i) {
      f[i] = fval[i];
   }
   Py_DECREF(farray);
   Py_DECREF(result);

   return 0; // Success
}

static
int eval_J(int n, int m, const void *params, const double *x, double *J) {
   // Recover our datatype
   const struct callback_data *data = (struct callback_data*) params;

   // Copy x into Python array
   npy_intp xdim[] = {n};
   PyArrayObject* xpy = (PyArrayObject*) PyArray_SimpleNew(1, xdim, NPY_DOUBLE);
   double* xval = (double*) PyArray_DATA(xpy);
   for(int i=0; i<n; ++i)
      xval[i] = x[i];

   // Call routine
   PyObject *arglist;
   if(data->params)  arglist = Py_BuildValue("(OO)", xpy, data->params);
   else              arglist = Py_BuildValue("(O)", xpy);
   PyObject *result = PyObject_CallObject(data->J, arglist);
   Py_DECREF(arglist);
   Py_DECREF(xpy);
   if(!result) return -1;

   // Extract result
   PyArrayObject* Jarray = (PyArrayObject*) PyArray_FROM_OTF(result, NPY_FLOAT64, NPY_ARRAY_IN_ARRAY);
   if(Jarray == NULL) {
      PyErr_SetString(PyExc_RuntimeError, "Error extracting array from f call");
      Py_DECREF(result);
      return -1;
   }
   if(PyArray_NDIM(Jarray) != 2) {
      PyErr_SetString(PyExc_RuntimeError, "J() must return rank-2 array");
      Py_DECREF(Jarray);
      Py_DECREF(result);
      return -2;
   }
   const double *Jval = (double*) PyArray_DATA(Jarray);
   for(int i=0; i<m*n; ++i) {
      J[i] = Jval[i];
   }
   Py_DECREF(Jarray);
   Py_DECREF(result);

   return 0; // Success
}

/*
 * x = ral_nlls.solve(x0, f, J=None, Hf=None, params=(), options={})
 */
static PyObject*
ral_nlls_solve(PyObject* self, PyObject* args, PyObject* keywds)
{
   PyObject *x0ptr=NULL, *options_ptr=NULL;
   PyObject *arglist=NULL, *result=NULL;
   PyArrayObject *x0=NULL, *f=NULL, *x=NULL;

   struct callback_data data;
   data.f = NULL; data.J = NULL; data.Hf = NULL; data.params = NULL;

   static char *kwlist[] = {"x0", "f", "J", "Hf", "params", "options", NULL};
   if(!PyArg_ParseTupleAndKeywords(args, keywds, "OOO|OOO", kwlist,
            &x0ptr,
            &data.f,
            &data.J,
            &data.Hf,
            &data.params,
            &options_ptr)
         )
      return NULL;

   /* x0 */
   x0 = (PyArrayObject*) PyArray_FROM_OTF(x0ptr, NPY_FLOAT64, NPY_ARRAY_IN_ARRAY);
   if(x0 == NULL) return NULL;
   if(PyArray_NDIM(x0) != 1) {
      PyErr_SetString(PyExc_RuntimeError, "x0 must be a rank-1 array");
      goto fail;
   }
   npy_intp* xdim = PyArray_DIMS(x0);
   int n = xdim[0];

   /* Determine m by making call to f */
   if(data.params)   arglist = Py_BuildValue("(OO)", x0, data.params);
   else              arglist = Py_BuildValue("(O)", x0);
   result = PyObject_CallObject(data.f, arglist);
   if(!result) goto fail;
   Py_DECREF(arglist); arglist=NULL;
   f = (PyArrayObject*) PyArray_FROM_OTF(result, NPY_FLOAT64, NPY_ARRAY_IN_ARRAY);
   if(f == NULL) {
      PyErr_SetString(PyExc_RuntimeError, "Error extracting array from f call");
      goto fail;
   }
   if(PyArray_NDIM(f) != 1) {
      PyErr_SetString(PyExc_RuntimeError, "f() must return rank-1 array");
      goto fail;
   }
   npy_intp* fdim = PyArray_DIMS(x0);
   int m = fdim[0];
   Py_DECREF(f); f=NULL;
   Py_DECREF(result); result=NULL;

   /* Construct return array x[] and set equal to x0 */
   x = (PyArrayObject*) PyArray_SimpleNew(1, xdim, NPY_DOUBLE);
   double* xval = (double*) PyArray_DATA(x);
   const double* x0val = (double*) PyArray_DATA(x0);
   for(int i=0; i<n; ++i)
      xval[i] = x0val[i];

   /* Call RAL_NLLS */
   struct ral_nlls_options options;
   ral_nlls_default_options_d(&options);
   struct ral_nlls_inform inform;
   nlls_solve_d(n, m, xval, eval_f, eval_J, NULL, &data, &options, &inform);
   switch(inform.status) {
      case 0: // Clean exit
         break;
      case -1: // Exceeded max itr
         PyErr_SetString(PyExc_RuntimeError,
               "Exceeded maximum number of iterations");
         goto fail;
      case -2: // Error return from evaluation of f/J/Hf
         // No error msg, allow existing one to propagate
         goto fail;
      case -3: // Unsupported choice of model
         PyErr_SetString(PyExc_RuntimeError,
               "Bad model");
         goto fail;
      default: ; // empty statement for language conformatity.
         char errmsg[100];
         sprintf(errmsg, "NLLS_SOLVE with unrecognised error code %d\n",
               inform.status);
         PyErr_SetString(PyExc_RuntimeError, errmsg);
         goto fail;
   }

   /* Free references and return solution */
   Py_DECREF(x0); x0=NULL;
   return (PyObject*) x;

   fail:
   Py_XDECREF(arglist); Py_XDECREF(result);
   Py_XDECREF(x0); Py_XDECREF(f); Py_XDECREF(x);
   return NULL;
}

static PyMethodDef RalNllsMethods[] = {
   {"solve", (PyCFunction)ral_nlls_solve, METH_VARARGS | METH_KEYWORDS,
    "Solve a non-linear least squares problem.\n"
    "   x = ral_nlls.solve(x0, f, J=None, Hf=None, params=(), options={})"
   },
   {NULL, NULL, 0, NULL} /* Sentinel */
};

PyMODINIT_FUNC
initral_nlls(void)
{
   (void) Py_InitModule("ral_nlls", RalNllsMethods);
   import_array();
}
