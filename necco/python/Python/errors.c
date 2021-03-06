
/* Error handling */

#include "Python.h"

void
PyErr_Restore(PyObject *type, PyObject *value, PyObject *traceback)
{
	PyThreadState *tstate = PyThreadState_GET();
	PyObject *oldtype, *oldvalue, *oldtraceback;

	/* Save these in locals to safeguard against recursive
	   invocation through Py_XDECREF */
	oldtype = tstate->curexc_type;
	oldvalue = tstate->curexc_value;
	oldtraceback = tstate->curexc_traceback;

	tstate->curexc_type = type;
	tstate->curexc_value = value;
	tstate->curexc_traceback = traceback;

	Py_XDECREF(oldtype);
	Py_XDECREF(oldvalue);
	Py_XDECREF(oldtraceback);
}

void
PyErr_SetObject(PyObject *exception, PyObject *value)
{
	Py_XINCREF(exception);
	Py_XINCREF(value);
	PyErr_Restore(exception, value, (PyObject *)NULL);
}

void
PyErr_SetNone(PyObject *exception)
{
	PyErr_SetObject(exception, (PyObject *)NULL);
}

void
PyErr_SetString(PyObject *exception, const char *string)
{
	PyObject *value = PyString_FromString(string);
	PyErr_SetObject(exception, value);
	Py_XDECREF(value);
}


PyObject *
PyErr_Occurred(void)
{
	PyThreadState *tstate = PyThreadState_GET();

	return tstate->curexc_type;
}


int
PyErr_GivenExceptionMatches(PyObject *err, PyObject *exc)
{
	if (err == NULL || exc == NULL) {
		/* maybe caused by "import exceptions" that failed early on */
		return 0;
	}
	if (PyTuple_Check(exc)) {
		int i, n;
		n = PyTuple_Size(exc);
		for (i = 0; i < n; i++) {
			/* Test recursively */
		     if (PyErr_GivenExceptionMatches(
			     err, PyTuple_GET_ITEM(exc, i)))
		     {
			     return 1;
		     }
		}
		return 0;
	}
	/* err might be an instance, so check its class. */
	//	if (PyInstance_Check(err))
	//	err = (PyObject*)((PyInstanceObject*)err)->in_class;

	//if (PyClass_Check(err) && PyClass_Check(exc))
	//	return PyClass_IsSubclass(err, exc);

	return err == exc;
}


int
PyErr_ExceptionMatches(PyObject *exc)
{
	return PyErr_GivenExceptionMatches(PyErr_Occurred(), exc);
}

/* Used in many places to normalize a raised exception, including in
   eval_code2(), do_raise(), and PyErr_Print()
*/
void
PyErr_NormalizeException(PyObject **exc, PyObject **val, PyObject **tb)
{
	PyObject *type = *exc;
	PyObject *value = *val;
	PyObject *inclass = NULL;
	PyObject *initial_tb = NULL;

	if (type == NULL) {
		/* There was no exception, so nothing to do. */
		return;
	}

	/* If PyErr_SetNone() was used, the value will have been actually
	   set to NULL.
	*/
	if (!value) {
		value = Py_None;
		Py_INCREF(value);
	}

	if (PyInstance_Check(value))
		inclass = (PyObject*)((PyInstanceObject*)value)->in_class;

	/* Normalize the exception so that if the type is a class, the
	   value will be an instance.
	*/
	if (PyClass_Check(type)) {
		/* if the value was not an instance, or is not an instance
		   whose class is (or is derived from) type, then use the
		   value as an argument to instantiation of the type
		   class.
		*/
		if (!inclass || !PyClass_IsSubclass(inclass, type)) {
			PyObject *args, *res;

			if (value == Py_None)
				args = Py_BuildValue("()");
			else if (PyTuple_Check(value)) {
				Py_INCREF(value);
				args = value;
			}
			else
				args = Py_BuildValue("(O)", value);

			if (args == NULL)
				goto finally;
			res = PyEval_CallObject(type, args);
			Py_DECREF(args);
			if (res == NULL)
				goto finally;
			Py_DECREF(value);
			value = res;
		}
		/* if the class of the instance doesn't exactly match the
		   class of the type, believe the instance
		*/
		else if (inclass != type) {
 			Py_DECREF(type);
			type = inclass;
			Py_INCREF(type);
		}
	}
	*exc = type;
	*val = value;
	return;
finally:
	Py_DECREF(type);
	Py_DECREF(value);
	/* If the new exception doesn't set a traceback and the old
	   exception had a traceback, use the old traceback for the
	   new exception.  It's better than nothing.
	*/
	initial_tb = *tb;
	PyErr_Fetch(exc, val, tb);
	if (initial_tb != NULL) {
		if (*tb == NULL)
			*tb = initial_tb;
		else
			Py_DECREF(initial_tb);
	}
	/* normalize recursively */
	PyErr_NormalizeException(exc, val, tb);
}

void
PyErr_Fetch(PyObject **p_type, PyObject **p_value, PyObject **p_traceback)
{
	PyThreadState *tstate = PyThreadState_Get();

	*p_type = tstate->curexc_type;
	*p_value = tstate->curexc_value;
	*p_traceback = tstate->curexc_traceback;

	tstate->curexc_type = NULL;
	tstate->curexc_value = NULL;
	tstate->curexc_traceback = NULL;
}

void
PyErr_Clear(void)
{
	PyErr_Restore(NULL, NULL, NULL);
}

/* Convenience functions to set a type error exception and return 0 */

int
PyErr_BadArgument(void)
{
	PyErr_SetString(PyExc_TypeError,
			"bad argument type for built-in operation");
	return 0;
}

PyObject *
PyErr_NoMemory(void)
{
	if (PyErr_ExceptionMatches(PyExc_MemoryError))
		/* already current */
		return NULL;

	/* raise the pre-allocated instance if it still exists */
	if (PyExc_MemoryErrorInst)
		PyErr_SetObject(PyExc_MemoryError, PyExc_MemoryErrorInst);
	else
		/* this will probably fail since there's no memory and hee,
		   hee, we have to instantiate this class
		*/
		PyErr_SetNone(PyExc_MemoryError);

	return NULL;
}

/* ... */

void
_PyErr_BadInternalCall(char *filename, int lineno)
{
	PyErr_Format(PyExc_SystemError,
		     "%s:%d: bad argument to internal function",
		     filename, lineno);
}

/* Remove the preprocessor macro for PyErr_BadInternalCall() so that we can
   export the entry point for existing object code: */
#undef PyErr_BadInternalCall
void
PyErr_BadInternalCall(void)
{
	PyErr_Format(PyExc_SystemError,
		     "bad argument to internal function");
}
#define PyErr_BadInternalCall() _PyErr_BadInternalCall(__FILE__, __LINE__)



PyObject *
PyErr_Format(PyObject *exception, const char *format, ...)
{
	va_list vargs;
	PyObject* string;

	va_start(vargs, format);

	string = PyString_FromFormatV(format, vargs);
	PyErr_SetObject(exception, string);
	Py_XDECREF(string);
	va_end(vargs);
	return NULL;
}

PyObject *
PyErr_NewException(char *name, PyObject *base, PyObject *dict)
{
	char *dot;
	PyObject *modulename = NULL;
	PyObject *classname = NULL;
	PyObject *mydict = NULL;
	PyObject *bases = NULL;
	PyObject *result = NULL;
	dot = strrchr(name, '.');
	if (dot == NULL) {
		PyErr_SetString(PyExc_SystemError,
			"PyErr_NewException: name must be module.class");
		return NULL;
	}
	if (base == NULL)
		base = PyExc_Exception;
	if (!PyClass_Check(base)) {
		/* Must be using string-based standard exceptions (-X) */
		return PyString_FromString(name);
	}
	if (dict == NULL) {
		dict = mydict = PyDict_New();
		if (dict == NULL)
			goto failure;
	}
	if (PyDict_GetItemString(dict, "__module__") == NULL) {
		modulename = PyString_FromStringAndSize(name, (int)(dot-name));
		if (modulename == NULL)
			goto failure;
		if (PyDict_SetItemString(dict, "__module__", modulename) != 0)
			goto failure;
	}
	classname = PyString_FromString(dot+1);
	if (classname == NULL)
		goto failure;
	bases = Py_BuildValue("(O)", base);
	if (bases == NULL)
		goto failure;
	result = PyClass_New(bases, dict, classname);
  failure:
	Py_XDECREF(bases);
	Py_XDECREF(mydict);
	Py_XDECREF(classname);
	Py_XDECREF(modulename);
	return result;
}

/* Call when an exception has occurred but there is no way for Python
   to handle it.  Examples: exception in __del__ or during GC. */
void
PyErr_WriteUnraisable(PyObject *obj)
{
  printf("Unraisable Exception\n");
  //	PyObject *f, *t, *v, *tb;
  //	PyErr_Fetch(&t, &v, &tb);
  //	f = PySys_GetObject("stderr");
  //	if (f != NULL) {
  //		PyFile_WriteString("Exception ", f);
  //		if (t) {
  //			PyFile_WriteObject(t, f, Py_PRINT_RAW);
  //			if (v && v != Py_None) {
  //				PyFile_WriteString(": ", f);
  //				PyFile_WriteObject(v, f, 0);
  //			}
  //		}
  //		PyFile_WriteString(" in ", f);
  //		PyFile_WriteObject(obj, f, 0);
  //		PyFile_WriteString(" ignored\n", f);
  //		PyErr_Clear(); /* Just in case */
  //	}
  //	Py_XDECREF(t);
  //	Py_XDECREF(v);
  //	Py_XDECREF(tb);
}
extern PyObject *PyModule_GetWarningsModule();

/* Function to issue a warning message; may raise an exception. */
int
PyErr_Warn(PyObject *category, char *message)
{
	PyObject *dict, *func = NULL;
	PyObject *warnings_module = PyModule_GetWarningsModule();

	if (warnings_module != NULL) {
		dict = PyModule_GetDict(warnings_module);
		func = PyDict_GetItemString(dict, "warn");
	}
	if (func == NULL) {
		printf("warning: %s\n", message);
		return 0;
	}
	else {
		PyObject *args, *res;

		if (category == NULL)
			category = PyExc_RuntimeWarning;
		args = Py_BuildValue("(sO)", message, category);
		if (args == NULL)
			return -1;
		res = PyEval_CallObject(func, args);
		Py_DECREF(args);
		if (res == NULL)
			return -1;
		Py_DECREF(res);
		return 0;
	}
}
