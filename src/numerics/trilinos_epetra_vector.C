// The libMesh Finite Element Library.
// Copyright (C) 2002-2025 Benjamin S. Kirk, John W. Peterson, Roy H. Stogner

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

// C++ includes
#include <limits>

// Local Includes
#include "libmesh/trilinos_epetra_vector.h"

#ifdef LIBMESH_TRILINOS_HAVE_EPETRA

#include "libmesh/dense_subvector.h"
#include "libmesh/dense_vector.h"
#include "libmesh/parallel.h"
#include "libmesh/trilinos_epetra_matrix.h"
#include "libmesh/utility.h"

// Trilinos Includes
#include "libmesh/ignore_warnings.h"
#include <Epetra_LocalMap.h>
#include <Epetra_Comm.h>
#include <Epetra_Map.h>
#include <Epetra_BlockMap.h>
#include <Epetra_Import.h>
#include <Epetra_Export.h>
#include <Epetra_Util.h>
#include <Epetra_IntSerialDenseVector.h>
#include <Epetra_SerialDenseVector.h>
#include <Epetra_Vector.h>
#include "libmesh/restore_warnings.h"

namespace libMesh
{

template <typename T>
T EpetraVector<T>::sum () const
{
  libmesh_assert(this->closed());

  const unsigned int nl = _vec->MyLength();

  T sum=0.0;

  T * values = _vec->Values();

  for (unsigned int i=0; i<nl; i++)
    sum += values[i];

  this->comm().sum(sum);

  return sum;
}

template <typename T>
Real EpetraVector<T>::l1_norm () const
{
  libmesh_assert(this->closed());

  Real value;

  _vec->Norm1(&value);

  return value;
}

template <typename T>
Real EpetraVector<T>::l2_norm () const
{
  libmesh_assert(this->closed());

  Real value;

  _vec->Norm2(&value);

  return value;
}

template <typename T>
Real EpetraVector<T>::linfty_norm () const
{
  libmesh_assert(this->closed());

  Real value;

  _vec->NormInf(&value);

  return value;
}

template <typename T>
NumericVector<T> &
EpetraVector<T>::operator += (const NumericVector<T> & v)
{
  libmesh_assert(this->closed());

  this->add(1., v);

  return *this;
}



template <typename T>
NumericVector<T> &
EpetraVector<T>::operator -= (const NumericVector<T> & v)
{
  libmesh_assert(this->closed());

  this->add(-1., v);

  return *this;
}


template <typename T>
NumericVector<T> &
EpetraVector<T>::operator *= (const NumericVector<T> & v)
{
  libmesh_assert(this->closed());
  libmesh_assert_equal_to(size(), v.size());

  const EpetraVector<T> & v_vec = cast_ref<const EpetraVector<T> &>(v);

  _vec->Multiply(1.0, *v_vec._vec, *_vec, 0.0);

  return *this;
}


template <typename T>
NumericVector<T> &
EpetraVector<T>::operator /= (const NumericVector<T> & v)
{
  libmesh_assert(this->closed());
  libmesh_assert_equal_to(size(), v.size());

  const EpetraVector<T> & v_vec = cast_ref<const EpetraVector<T> &>(v);

  _vec->ReciprocalMultiply(1.0, *v_vec._vec, *_vec, 0.0);

  return *this;
}




template <typename T>
void EpetraVector<T>::set (const numeric_index_type i_in, const T value_in)
{
  int i = static_cast<int> (i_in);
  T value = value_in;

  libmesh_assert_less (i_in, this->size());

  std::scoped_lock lock(this->_numeric_vector_mutex);
  ReplaceGlobalValues(1, &i, &value);

  this->_is_closed = false;
}



template <typename T>
void EpetraVector<T>::reciprocal()
{
  // The Epetra::reciprocal() function takes a constant reference to *another* vector,
  // and fills _vec with its reciprocal.  Does that mean we can't pass *_vec as the
  // argument?
  // _vec->reciprocal( *_vec );

  // Alternatively, compute the reciprocal by hand... see also the add(T) member that does this...
  const unsigned int nl = _vec->MyLength();

  T * values = _vec->Values();

  for (unsigned int i=0; i<nl; i++)
    {
      // Don't divide by zero (maybe only check this in debug mode?)
      libmesh_error_msg_if(std::abs(values[i]) < std::numeric_limits<T>::min(),
                           "Error, divide by zero in DistributedVector<T>::reciprocal()!");

      values[i] = 1. / values[i];
    }

  // Leave the vector in a closed state...
  this->close();
}



template <typename T>
void EpetraVector<T>::conjugate()
{
  // EPetra is real, rendering this a no-op.
}



template <typename T>
void EpetraVector<T>::add (const numeric_index_type i_in, const T value_in)
{
  int i = static_cast<int> (i_in);
  T value = value_in;

  libmesh_assert_less (i_in, this->size());

  std::scoped_lock lock(this->_numeric_vector_mutex);
  SumIntoGlobalValues(1, &i, &value);

  this->_is_closed = false;
}



template <typename T>
void EpetraVector<T>::add_vector (const T * v,
                                  const std::vector<numeric_index_type> & dof_indices)
{
  libmesh_assert_equal_to (sizeof(numeric_index_type), sizeof(int));

  std::scoped_lock lock(this->_numeric_vector_mutex);
  SumIntoGlobalValues (cast_int<numeric_index_type>(dof_indices.size()),
                       numeric_trilinos_cast(dof_indices.data()),
                       const_cast<T *>(v));
}



// TODO: fill this in after creating an EpetraMatrix
template <typename T>
void EpetraVector<T>::add_vector (const NumericVector<T> & v_in,
                                  const SparseMatrix<T> & A_in)
{
  const EpetraVector<T> * v = cast_ptr<const EpetraVector<T> *>(&v_in);
  const EpetraMatrix<T> * A = cast_ptr<const EpetraMatrix<T> *>(&A_in);

  // FIXME - does Trilinos let us do this *without* memory allocation?
  std::unique_ptr<NumericVector<T>> temp = v->zero_clone();
  EpetraVector<T> * temp_v = cast_ptr<EpetraVector<T> *>(temp.get());
  A->mat()->Multiply(false, *v->_vec, *temp_v->_vec);
  *this += *temp;
}



// TODO: fill this in after creating an EpetraMatrix
template <typename T>
void EpetraVector<T>::add_vector_transpose (const NumericVector<T> & /* v_in */,
                                            const SparseMatrix<T> & /* A_in */)
{
  libmesh_not_implemented();
}



template <typename T>
void EpetraVector<T>::add (const T v_in)
{
  const unsigned int nl = _vec->MyLength();

  T * values = _vec->Values();

  for (unsigned int i=0; i<nl; i++)
    values[i]+=v_in;

  this->_is_closed = false;
}


template <typename T>
void EpetraVector<T>::add (const NumericVector<T> & v)
{
  this->add (1., v);
}


template <typename T>
void EpetraVector<T>::add (const T a_in, const NumericVector<T> & v_in)
{
  const EpetraVector<T> * v = cast_ptr<const EpetraVector<T> *>(&v_in);

  libmesh_assert_equal_to (this->size(), v->size());

  _vec->Update(a_in,*v->_vec, 1.);
}



template <typename T>
void EpetraVector<T>::insert (const T * v,
                              const std::vector<numeric_index_type> & dof_indices)
{
  libmesh_assert_equal_to (sizeof(numeric_index_type), sizeof(int));

  std::scoped_lock lock(this->_numeric_vector_mutex);
  ReplaceGlobalValues (cast_int<numeric_index_type>(dof_indices.size()),
                       numeric_trilinos_cast(dof_indices.data()),
                       const_cast<T *>(v));
  this->_is_closed = false;
}



template <typename T>
void EpetraVector<T>::scale (const T factor_in)
{
  _vec->Scale(factor_in);
}

template <typename T>
void EpetraVector<T>::abs()
{
  _vec->Abs(*_vec);
}


template <typename T>
T EpetraVector<T>::dot (const NumericVector<T> & v_in) const
{
  const EpetraVector<T> * v = cast_ptr<const EpetraVector<T> *>(&v_in);

  T result=0.0;

  _vec->Dot(*v->_vec, &result);

  return result;
}


template <typename T>
void EpetraVector<T>::pointwise_mult (const NumericVector<T> & vec1,
                                      const NumericVector<T> & vec2)
{
  const EpetraVector<T> * v1 = cast_ptr<const EpetraVector<T> *>(&vec1);
  const EpetraVector<T> * v2 = cast_ptr<const EpetraVector<T> *>(&vec2);

  _vec->Multiply(1.0, *v1->_vec, *v2->_vec, 0.0);
}


template <typename T>
NumericVector<T> &
EpetraVector<T>::operator = (const T s_in)
{
  _vec->PutScalar(s_in);

  return *this;
}



template <typename T>
NumericVector<T> &
EpetraVector<T>::operator = (const NumericVector<T> & /*v_in*/)
{
  // This function could be implemented in terms of the copy
  // assignment operator (see other NumericVector subclasses) but that
  // function is currently deleted so calling this function is an error.
  // const EpetraVector<T> * v = cast_ptr<const EpetraVector<T> *>(&v_in);
  // *this = *v;
  libmesh_not_implemented();
  return *this;
}



template <typename T>
NumericVector<T> &
EpetraVector<T>::operator = (const std::vector<T> & v)
{
  T * values = _vec->Values();

  /**
   * Case 1:  The vector is the same size of
   * The global vector.  Only add the local components.
   */
  if (this->size() == v.size())
    {
      const unsigned int nl=this->local_size();
      const unsigned int fli=this->first_local_index();

      for (unsigned int i=0;i<nl;i++)
        values[i]=v[fli+i];
    }

  /**
   * Case 2: The vector is the same size as our local
   * piece.  Insert directly to the local piece.
   */
  else
    {
      libmesh_assert_equal_to (v.size(), this->local_size());

      const unsigned int nl=this->local_size();

      for (unsigned int i=0;i<nl;i++)
        values[i]=v[i];
    }

  return *this;
}



template <typename T>
void EpetraVector<T>::localize (NumericVector<T> & v_local_in) const
{
  EpetraVector<T> * v_local = cast_ptr<EpetraVector<T> *>(&v_local_in);

  Epetra_Map rootMap = Epetra_Util::Create_Root_Map( *_map, -1);
  v_local->_vec->ReplaceMap(rootMap);

  Epetra_Import importer(v_local->_vec->Map(), *_map);
  v_local->_vec->Import(*_vec, importer, Insert);
}



template <typename T>
void EpetraVector<T>::localize (NumericVector<T> & v_local_in,
                                const std::vector<numeric_index_type> & /* send_list */) const
{
  // TODO: optimize to sync only the send list values
  this->localize(v_local_in);

  //   EpetraVector<T> * v_local =
  //   cast_ptr<EpetraVector<T> *>(&v_local_in);

  //   libmesh_assert(this->_map.get());
  //   libmesh_assert(v_local->_map.get());
  //   libmesh_assert_equal_to (v_local->local_size(), this->size());
  //   libmesh_assert_less_equal (send_list.size(), v_local->size());

  //   Epetra_Import importer (*v_local->_map, *this->_map);

  //   v_local->_vec->Import (*this->_vec, importer, Insert);
}



template <typename T>
void EpetraVector<T>::localize (std::vector<T> & v_local,
                                const std::vector<numeric_index_type> & indices) const
{
  // Create a "replicated" map for importing values.  This is
  // equivalent to creating a general Epetra_Map with
  // NumGlobalElements == NumMyElements.
  Epetra_LocalMap import_map(static_cast<int>(indices.size()),
                             /*IndexBase=*/0,
                             _map->Comm());

  // Get a pointer to the list of global elements for the map, and set
  // all the values from indices.
  int * import_map_global_elements = import_map.MyGlobalElements();
  for (auto i : index_range(indices))
    import_map_global_elements[i] = indices[i];

  // Create a new EpetraVector to import values into.
  Epetra_Vector import_vector(import_map);

  // Set up an "Import" object which associates the two maps.
  Epetra_Import import_object(import_map, *_map);

  // Import the values
  import_vector.Import(*_vec, import_object, Insert);

  // Get a pointer to the imported values array and the length of the
  // array.
  T * values = import_vector.Values();
  int import_vector_length = import_vector.MyLength();

  // Copy the imported values into v_local
  v_local.resize(import_vector_length);
  for (int i=0; i<import_vector_length; ++i)
    v_local[i] = values[i];
}



template <typename T>
void EpetraVector<T>::localize (const numeric_index_type first_local_idx,
                                const numeric_index_type last_local_idx,
                                const std::vector<numeric_index_type> & send_list)
{
  // Only good for serial vectors.
  libmesh_assert_equal_to (this->size(), this->local_size());
  libmesh_assert_greater (last_local_idx, first_local_idx);
  libmesh_assert_less_equal (send_list.size(), this->size());
  libmesh_assert_less (last_local_idx, this->size());

  const unsigned int my_size       = this->size();
  const unsigned int my_local_size = (last_local_idx - first_local_idx + 1);

  // Don't bother for serial cases
  if ((first_local_idx == 0) &&
      (my_local_size == my_size))
    return;

  // Build a parallel vector, initialize it with the local
  // parts of (*this)
  EpetraVector<T> parallel_vec(this->comm(), PARALLEL);

  parallel_vec.init (my_size, my_local_size, true, PARALLEL);

  // Copy part of *this into the parallel_vec
  for (numeric_index_type i=first_local_idx; i<=last_local_idx; i++)
    parallel_vec.set(i,this->el(i));

  // localize like normal
  parallel_vec.close();
  parallel_vec.localize (*this, send_list);
  this->close();
}



template <typename T>
void EpetraVector<T>::localize (std::vector<T> & v_local) const
{
  // This function must be run on all processors at once
  parallel_object_only();

  const unsigned int n  = this->size();
  const unsigned int nl = this->local_size();

  libmesh_assert(this->_vec);

  v_local.clear();
  v_local.reserve(n);

  // build up my local part
  for (unsigned int i=0; i<nl; i++)
    v_local.push_back((*this->_vec)[i]);

  this->comm().allgather (v_local);
}



template <typename T>
void EpetraVector<T>::localize_to_one (std::vector<T> &  v_local,
                                       const processor_id_type pid) const
{
  // This function must be run on all processors at once
  parallel_object_only();

  const unsigned int n  = this->size();
  const unsigned int nl = this->local_size();

  libmesh_assert_less (pid, this->n_processors());
  libmesh_assert(this->_vec);

  v_local.clear();
  v_local.reserve(n);


  // build up my local part
  for (unsigned int i=0; i<nl; i++)
    v_local.push_back((*this->_vec)[i]);

  this->comm().gather (pid, v_local);
}


/*********************************************************************
 * The following were copied (and slightly modified) from
 * Epetra_FEVector.h in order to allow us to use a standard
 * Epetra_Vector... which is more compatible with other Trilinos
 * packages such as NOX.  All of this code is originally under LGPL
 *********************************************************************/

//----------------------------------------------------------------------------
template <typename T>
int EpetraVector<T>::SumIntoGlobalValues(int numIDs,
                                         const int * GIDs,
                                         const double * values)
{
  return( inputValues( numIDs, GIDs, values, true) );
}

//----------------------------------------------------------------------------
template <typename T>
int EpetraVector<T>::SumIntoGlobalValues(const Epetra_IntSerialDenseVector & GIDs,
                                         const Epetra_SerialDenseVector & values)
{
  if (GIDs.Length() != values.Length()) {
    return(-1);
  }

  return( inputValues( GIDs.Length(), GIDs.Values(), values.Values(), true) );
}

//----------------------------------------------------------------------------
template <typename T>
int EpetraVector<T>::SumIntoGlobalValues(int numIDs,
                                         const int * GIDs,
                                         const int * numValuesPerID,
                                         const double * values)
{
  return( inputValues( numIDs, GIDs, numValuesPerID, values, true) );
}

//----------------------------------------------------------------------------
template <typename T>
int EpetraVector<T>::ReplaceGlobalValues(int numIDs,
                                         const int * GIDs,
                                         const double * values)
{
  return( inputValues( numIDs, GIDs, values, false) );
}

//----------------------------------------------------------------------------
template <typename T>
int EpetraVector<T>::ReplaceGlobalValues(const Epetra_IntSerialDenseVector & GIDs,
                                         const Epetra_SerialDenseVector & values)
{
  if (GIDs.Length() != values.Length()) {
    return(-1);
  }

  return( inputValues( GIDs.Length(), GIDs.Values(), values.Values(), false) );
}

//----------------------------------------------------------------------------
template <typename T>
int EpetraVector<T>::ReplaceGlobalValues(int numIDs,
                                         const int * GIDs,
                                         const int * numValuesPerID,
                                         const double * values)
{
  return( inputValues( numIDs, GIDs, numValuesPerID, values, false) );
}

//----------------------------------------------------------------------------
template <typename T>
int EpetraVector<T>::inputValues(int numIDs,
                                 const int * GIDs,
                                 const double * values,
                                 bool accumulate)
{
  if (accumulate) {
    libmesh_assert(last_edit == 0 || last_edit == 2);
    last_edit = 2;
  } else {
    libmesh_assert(last_edit == 0 || last_edit == 1);
    last_edit = 1;
  }

  //Important note!! This method assumes that there is only 1 point
  //associated with each element.

  for (int i=0; i<numIDs; ++i) {
    if (_vec->Map().MyGID(GIDs[i])) {
      if (accumulate) {
        _vec->SumIntoGlobalValue(GIDs[i], 0, 0, values[i]);
      }
      else {
        _vec->ReplaceGlobalValue(GIDs[i], 0, 0, values[i]);
      }
    }
    else {
      if (!ignoreNonLocalEntries_) {
        EPETRA_CHK_ERR( inputNonlocalValue(GIDs[i], values[i], accumulate) );
      }
    }
  }

  return(0);
}

//----------------------------------------------------------------------------
template <typename T>
int EpetraVector<T>::inputValues(int numIDs,
                                 const int * GIDs,
                                 const int * numValuesPerID,
                                 const double * values,
                                 bool accumulate)
{
  if (accumulate) {
    libmesh_assert(last_edit == 0 || last_edit == 2);
    last_edit = 2;
  } else {
    libmesh_assert(last_edit == 0 || last_edit == 1);
    last_edit = 1;
  }

  int offset=0;
  for (int i=0; i<numIDs; ++i) {
    int numValues = numValuesPerID[i];
    if (_vec->Map().MyGID(GIDs[i])) {
      if (accumulate) {
        for (int j=0; j<numValues; ++j) {
          _vec->SumIntoGlobalValue(GIDs[i], j, 0, values[offset+j]);
        }
      }
      else {
        for (int j=0; j<numValues; ++j) {
          _vec->ReplaceGlobalValue(GIDs[i], j, 0, values[offset+j]);
        }
      }
    }
    else {
      if (!ignoreNonLocalEntries_) {
        EPETRA_CHK_ERR( inputNonlocalValues(GIDs[i], numValues,
                                            &(values[offset]), accumulate) );
      }
    }
    offset += numValues;
  }

  return(0);
}

//----------------------------------------------------------------------------
template <typename T>
int EpetraVector<T>::inputNonlocalValue(int GID, double value, bool accumulate)
{
  int insertPoint = -1;

  //find offset of GID in nonlocalIDs_
  int offset = Epetra_Util_binary_search(GID, nonlocalIDs_, numNonlocalIDs_,
                                         insertPoint);
  if (offset >= 0) {
    //if offset >= 0
    //  put value in nonlocalCoefs_[offset][0]

    if (accumulate) {
      nonlocalCoefs_[offset][0] += value;
    }
    else {
      nonlocalCoefs_[offset][0] = value;
    }
  }
  else {
    //else
    //  insert GID in nonlocalIDs_
    //  insert 1   in nonlocalElementSize_
    //  insert value in nonlocalCoefs_

    int tmp1 = numNonlocalIDs_;
    int tmp2 = allocatedNonlocalLength_;
    int tmp3 = allocatedNonlocalLength_;
    EPETRA_CHK_ERR( Epetra_Util_insert(GID, insertPoint, nonlocalIDs_,
                                       tmp1, tmp2) );
    --tmp1;
    EPETRA_CHK_ERR( Epetra_Util_insert(1, insertPoint, nonlocalElementSize_,
                                       tmp1, tmp3) );
    double * values = new double[1];
    values[0] = value;
    EPETRA_CHK_ERR( Epetra_Util_insert(values, insertPoint, nonlocalCoefs_,
                                       numNonlocalIDs_, allocatedNonlocalLength_) );
  }

  return(0);
}

//----------------------------------------------------------------------------
template <typename T>
int EpetraVector<T>::inputNonlocalValues(int GID,
                                         int numValues,
                                         const double * values,
                                         bool accumulate)
{
  int insertPoint = -1;

  //find offset of GID in nonlocalIDs_
  int offset = Epetra_Util_binary_search(GID, nonlocalIDs_, numNonlocalIDs_,
                                         insertPoint);
  if (offset >= 0) {
    //if offset >= 0
    //  put value in nonlocalCoefs_[offset][0]

    if (numValues != nonlocalElementSize_[offset]) {
      libMesh::err << "Epetra_FEVector ERROR: block-size for GID " << GID << " is "
                   << numValues<<" which doesn't match previously set block-size of "
                   << nonlocalElementSize_[offset] << std::endl;
      return(-1);
    }

    if (accumulate) {
      for (int j=0; j<numValues; ++j) {
        nonlocalCoefs_[offset][j] += values[j];
      }
    }
    else {
      for (int j=0; j<numValues; ++j) {
        nonlocalCoefs_[offset][j] = values[j];
      }
    }
  }
  else {
    //else
    //  insert GID in nonlocalIDs_
    //  insert numValues   in nonlocalElementSize_
    //  insert values in nonlocalCoefs_

    int tmp1 = numNonlocalIDs_;
    int tmp2 = allocatedNonlocalLength_;
    int tmp3 = allocatedNonlocalLength_;
    EPETRA_CHK_ERR( Epetra_Util_insert(GID, insertPoint, nonlocalIDs_,
                                       tmp1, tmp2) );
    --tmp1;
    EPETRA_CHK_ERR( Epetra_Util_insert(numValues, insertPoint, nonlocalElementSize_,
                                       tmp1, tmp3) );
    double * newvalues = new double[numValues];
    for (int j=0; j<numValues; ++j) {
      newvalues[j] = values[j];
    }
    EPETRA_CHK_ERR( Epetra_Util_insert(newvalues, insertPoint, nonlocalCoefs_,
                                       numNonlocalIDs_, allocatedNonlocalLength_) );
  }

  return(0);
}

//----------------------------------------------------------------------------
template <typename T>
int EpetraVector<T>::GlobalAssemble(Epetra_CombineMode mode)
{
  //In this method we need to gather all the non-local (overlapping) data
  //that's been input on each processor, into the (probably) non-overlapping
  //distribution defined by the map that 'this' vector was constructed with.

  //We don't need to do anything if there's only one processor or if
  //ignoreNonLocalEntries_ is true.
  if (_vec->Map().Comm().NumProc() < 2 || ignoreNonLocalEntries_) {
    return(0);
  }



  //First build a map that describes the data in nonlocalIDs_/nonlocalCoefs_.
  //We'll use the arbitrary distribution constructor of Map.

  Epetra_BlockMap sourceMap(-1, numNonlocalIDs_,
                            nonlocalIDs_, nonlocalElementSize_,
                            _vec->Map().IndexBase(), _vec->Map().Comm());

  //Now build a vector to hold our nonlocalCoefs_, and to act as the source-
  //vector for our import operation.
  Epetra_MultiVector nonlocalVector(sourceMap, 1);

  int i,j;
  for (i=0; i<numNonlocalIDs_; ++i) {
    for (j=0; j<nonlocalElementSize_[i]; ++j) {
      nonlocalVector.ReplaceGlobalValue(nonlocalIDs_[i], j, 0,
                                        nonlocalCoefs_[i][j]);
    }
  }

  Epetra_Export exporter(sourceMap, _vec->Map());

  EPETRA_CHK_ERR( _vec->Export(nonlocalVector, exporter, mode) );

  destroyNonlocalData();

  return(0);
}

#include <libmesh/ignore_warnings.h> // deprecated-copy in Epetra_Vector

//----------------------------------------------------------------------------
template <typename T>
void EpetraVector<T>::FEoperatorequals(const EpetraVector & source)
{
  (*_vec) = *(source._vec);

  destroyNonlocalData();

  if (source.allocatedNonlocalLength_ > 0) {
    allocatedNonlocalLength_ = source.allocatedNonlocalLength_;
    numNonlocalIDs_ = source.numNonlocalIDs_;
    nonlocalIDs_ = new int[allocatedNonlocalLength_];
    nonlocalElementSize_ = new int[allocatedNonlocalLength_];
    nonlocalCoefs_ = new double *[allocatedNonlocalLength_];
    for (int i=0; i<numNonlocalIDs_; ++i) {
      int elemSize = source.nonlocalElementSize_[i];
      nonlocalCoefs_[i] = new double[elemSize];
      nonlocalIDs_[i] = source.nonlocalIDs_[i];
      nonlocalElementSize_[i] = elemSize;
      for (int j=0; j<elemSize; ++j) {
        nonlocalCoefs_[i][j] = source.nonlocalCoefs_[i][j];
      }
    }
  }
}

#include <libmesh/restore_warnings.h>

//----------------------------------------------------------------------------
template <typename T>
void EpetraVector<T>::destroyNonlocalData()
{
  if (allocatedNonlocalLength_ > 0) {
    delete [] nonlocalIDs_;
    delete [] nonlocalElementSize_;
    nonlocalIDs_ = nullptr;
    nonlocalElementSize_ = nullptr;
    for (int i=0; i<numNonlocalIDs_; ++i) {
      delete [] nonlocalCoefs_[i];
    }
    delete [] nonlocalCoefs_;
    nonlocalCoefs_ = nullptr;
    numNonlocalIDs_ = 0;
    allocatedNonlocalLength_ = 0;
  }
  return;
}


//------------------------------------------------------------------
// Explicit instantiations
template class LIBMESH_EXPORT EpetraVector<Number>;

} // namespace libMesh

#endif // LIBMESH_TRILINOS_HAVE_EPETRA
