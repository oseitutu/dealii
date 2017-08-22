// ---------------------------------------------------------------------
//
// Copyright (C) 1998 - 2017 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE at
// the top level of the deal.II distribution.
//
// ---------------------------------------------------------------------

#ifndef dealii_vector_memory_h
#define dealii_vector_memory_h


#include <deal.II/base/config.h>
#include <deal.II/base/smartpointer.h>
#include <deal.II/base/logstream.h>
#include <deal.II/base/thread_management.h>
#include <deal.II/lac/vector.h>

#include <vector>
#include <iostream>
#include <memory>

DEAL_II_NAMESPACE_OPEN


/*!@addtogroup VMemory */
/*@{*/

/**
 * Memory management base class for vectors. This is an abstract base class
 * used, among other places, by all iterative methods to allocate space for
 * auxiliary vectors.
 *
 * The purpose of this class is as follows: in iterative solvers and other
 * places, one needs to allocate temporary storage for vectors, for example
 * for auxiliary vectors. One could allocate and release them anew every time,
 * but this may be expensive in some situations if it has to happen very
 * frequently. A common case for this is when an iterative method is used to
 * invert a matrix in each iteration an outer solver, such as when inverting a
 * matrix block for a Schur complement solver.
 *
 * In such situations, allocating and deallocating vectors anew in each call
 * to the inner solver is expensive and leads to memory fragmentation. The
 * present class allows to avoid this by offering an interface that other
 * classes can use to allocate and deallocate vectors. Different derived
 * classes then implement different strategies to provide temporary storage
 * vectors to using classes.
 *
 * For example, the PrimitiveVectorMemory class simply allocates and
 * deallocated vectors each time it is asked for a vector. It is an
 * appropriate implementation to use for iterative solvers that are called
 * only once, or very infrequently.
 *
 * On the other hand, the GrowingVectorMemory class never returns memory space
 * to the operating system memory management subsystem during its lifetime; it
 * only marks them as unused and allows them to be reused next time a vector
 * is requested.
 *
 * Yet other classes, when implemented, could provide even other strategies
 * for memory management.
 *
 * @author Guido Kanschat, 1998-2003
 */
template <typename VectorType = dealii::Vector<double> >
class VectorMemory : public Subscriptor
{
public:

  /**
   * Virtual destructor. This destructor is declared @p virtual to allow
   * destroying objects of derived type through pointers to this base
   * class.
   */
  virtual ~VectorMemory () = default;

  /**
   * Return a pointer to a new vector. The number of elements or their
   * subdivision into blocks (if applicable) is unspecified and users of this
   * function should reset vectors to their proper size. The same holds for
   * the contents of vectors: they are unspecified.
   */
  virtual VectorType *alloc () = 0;

  /**
   * Return a vector and indicate that it is not going to be used any further
   * by the instance that called alloc() to get a pointer to it.
   */
  virtual void free (const VectorType *const) = 0;

  /**
   * @addtogroup Exceptions
   * @{
   */

  /**
   * Vector was not allocated from this memory pool.
   */
  DeclExceptionMsg(ExcNotAllocatedHere,
                   "You are trying to deallocate a vector from a memory pool, but this "
                   "vector has not actually been allocated by the same pool before.");

  //@}

  /**
   * A class that looks like a pointer for all practical purposes and that
   * upon construction time allocates a vector from a VectorMemory object
   * (or an object of a class derived from VectorMemory) that is passed
   * to the constructor of this class. The destructor then automatically
   * returns the vector's ownership to the same VectorMemory object.
   *
   * Pointers of this type are therefore safe in the sense that they automatically
   * call VectorMemory::free() when they are destroyed, whether that happens
   * at the end of a code block or because local variables are destroyed during
   * exception unwinding. These kinds of object thus relieve the user from
   * using vector management functions explicitly.
   *
   * In many senses, this class acts like <code>std::unique_ptr</code> in that
   * it is the unique owner of a chunk of memory that it frees upon destruction.
   * The main differences to <code>std::unique_ptr</code> are (i) that it
   * allocates memory from a memory pool upon construction, and (ii) that the
   * memory is not destroyed using `operator delete` but returned to the
   * VectorMemory pool.
   *
   * @author Guido Kanschat, 2009
   */
  class Pointer : public std::unique_ptr<VectorType, std::function<void (VectorType *)> >
  {
  public:
    /**
     * Constructor, automatically allocating a vector from @p mem.
     */
    Pointer(VectorMemory<VectorType> &mem);

    /**
     * Destructor, automatically releasing the vector from the memory #pool.
     */
    ~Pointer() = default;
  };
};



/**
 * Simple memory management. See the documentation of the base class for a
 * description of its purpose.
 *
 * This class allocates and deletes vectors as needed from the global heap,
 * i.e. performs no specially adapted actions for memory management.
 */
template <typename VectorType = dealii::Vector<double> >
class PrimitiveVectorMemory : public VectorMemory<VectorType>
{
public:
  /**
   * Return a pointer to a new vector. The number of elements or their
   * subdivision into blocks (if applicable) is unspecified and users of this
   * function should reset vectors to their proper size. The same holds for
   * the contents of vectors: they are unspecified.
   *
   * For the present class, calling this function will allocate a new vector
   * on the heap and returning a pointer to it.
   */
  virtual VectorType *alloc ();

  /**
   * Return a vector and indicate that it is not going to be used any further
   * by the instance that called alloc() to get a pointer to it.
   *
   *
   * For the present class, this means that the vector is returned to the
   * global heap.
   */
  virtual void free (const VectorType *const v);
};



/**
 * A pool based memory management class. See the documentation of the base
 * class for a description of its purpose.
 *
 * Each time a vector is requested from this class, it checks if it has one
 * available and returns its address, or allocates a new one on the heap. If a
 * vector is returned, through the free() member function, it doesn't return
 * it to the operating system memory subsystem, but keeps it around unused for
 * later use if alloc() is called again, or until the object is destroyed. The
 * class therefore avoid the overhead of repeatedly allocating memory on the
 * heap if temporary vectors are required and released frequently; on the
 * other hand, it doesn't release once-allocated memory at the earliest
 * possible time and may therefore lead to an increased overall memory
 * consumption.
 *
 * All GrowingVectorMemory objects of the same vector type use the same memory
 * Pool. Therefore, functions can create such a VectorMemory object whenever
 * needed without performance penalty. A drawback of this policy might be that
 * vectors once allocated are only released at the end of the program run.
 * Nevertheless, the since they are reused, this should be of no concern.
 * Additionally, the destructor of the Pool warns about memory leaks.
 *
 * @author Guido Kanschat, 1999, 2007
 */
template <typename VectorType = dealii::Vector<double> >
class GrowingVectorMemory : public VectorMemory<VectorType>
{
public:
  /**
   * Declare type for container size.
   */
  typedef types::global_dof_index size_type;

  /**
   * Constructor.  The argument allows to preallocate a certain number of
   * vectors. The default is not to do this.
   */
  GrowingVectorMemory (const size_type initial_size = 0,
                       const bool log_statistics = false);

  /**
   * Destructor. Release all vectors. This destructor also offers some
   * statistic on the number of allocated vectors.
   *
   * The log file will also contain a warning message, if there are allocated
   * vectors left.
   */
  virtual ~GrowingVectorMemory();

  /**
   * Return a pointer to a new vector. The number of elements or their
   * subdivision into blocks (if applicable) is unspecified and users of this
   * function should reset vectors to their proper size. The same holds for
   * the contents of vectors: they are unspecified.
   */
  virtual VectorType *alloc ();

  /**
   * Return a vector and indicate that it is not going to be used any further
   * by the instance that called alloc() to get a pointer to it.
   *
   * For the present class, this means retaining the vector for later reuse by
   * the alloc() method.
   */
  virtual void free (const VectorType *const);

  /**
   * Release all vectors that are not currently in use.
   */
  static void release_unused_memory ();

  /**
   * Memory consumed by this class and all currently allocated vectors.
   */
  virtual std::size_t memory_consumption() const;

private:
  /**
   * A type that describes this entries of an array that represents
   * the vectors stored by this object. The first component of the pair
   * is be a flag telling whether the vector is used, the second
   * a pointer to the vector itself.
   */
  typedef std::pair<bool, std::unique_ptr<VectorType> > entry_type;

  /**
   * The class providing the actual storage for the memory pool.
   *
   * This is where the actual storage for GrowingVectorMemory is provided.
   * Only one of these pools is used for each vector type, thus allocating all
   * vectors from the same storage.
   *
   * @author Guido Kanschat, 2007, Wolfgang Bangerth 2017.
   */
  struct Pool
  {
    /**
     * Standard constructor creating an empty pool
     */
    Pool();

    /**
     * Destructor.
     */
    ~Pool();

    /**
     * Create data vector; does nothing after first initialization
     */
    void initialize(const size_type size);

    /**
     * Pointer to the storage object
     */
    std::vector<entry_type> *data;
  };

  /**
   * Array of allocated vectors.
   */
  static Pool pool;

  /**
   * Overall number of allocations. Only used for bookkeeping and to generate
   * output at the end of an object's lifetime.
   */
  size_type total_alloc;

  /**
   * Number of vectors currently allocated in this object; used for detecting
   * memory leaks.
   */
  size_type current_alloc;

  /**
   * A flag controlling the logging of statistics by the destructor.
   */
  bool log_statistics;

  /**
   * Mutex to synchronise access to internal data of this object from multiple
   * threads.
   */
  static Threads::Mutex mutex;
};



namespace internal
{
  namespace GrowingVectorMemory
  {
    void release_all_unused_memory();
  }
}

/*@}*/

#ifndef DOXYGEN
/* --------------------- inline functions ---------------------- */


template <typename VectorType>
inline
VectorMemory<VectorType>::Pointer::Pointer(VectorMemory<VectorType> &mem)
  :
  std::unique_ptr<VectorType, std::function<void (VectorType *)> >
  (mem.alloc(), [&mem](VectorType *v)
{
  mem.free(v);
})
{}



template <typename VectorType>
VectorType *
PrimitiveVectorMemory<VectorType>::alloc ()
{
  return new VectorType();
}



template <typename VectorType>
void
PrimitiveVectorMemory<VectorType>::free (const VectorType *const v)
{
  delete v;
}




#endif // DOXYGEN

DEAL_II_NAMESPACE_CLOSE

#endif
