/* $Id$ */
/* Author: Wolfgang Bangerth, University of Heidelberg, 1999 */

/*    $Id$       */
/*    Version: $Name$                                          */
/*                                                                */
/*    Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006 by the deal.II authors */
/*                                                                */
/*    This file is subject to QPL and may not be  distributed     */
/*    without copyright and license information. Please refer     */
/*    to the file deal.II/doc/license.html for the  text  and     */
/*    further information on this license.                        */

                                 // @sect3{Include files}

				 // The first few (many?) include
				 // files have already been used in
				 // the previous example, so we will
				 // not explain their meaning here
				 // again.
#include <grid/tria.h>
#include <dofs/dof_handler.h>
#include <grid/grid_generator.h>
#include <grid/tria_accessor.h>
#include <grid/tria_iterator.h>
#include <dofs/dof_accessor.h>
#include <fe/fe_q.h>
#include <dofs/dof_tools.h>
#include <fe/fe_values.h>
#include <base/quadrature_lib.h>
#include <base/function.h>
#include <numerics/vectors.h>
#include <numerics/matrices.h>
#include <lac/vector.h>
#include <lac/full_matrix.h>
#include <lac/sparse_matrix.h>
#include <lac/solver_cg.h>
#include <lac/precondition.h>

#include <numerics/data_out.h>
#include <fstream>
#include <iostream>

				 // This is new, however: in the previous
				 // example we got some unwanted output from
				 // the linear solvers. If we want to suppress
				 // it, we have to include this file and add a
				 // single line somewhere to the program (see
				 // the main() function below for that):
#include <base/logstream.h>


                                 // @sect3{The ``LaplaceProblem'' class template}

				 // This is again the same
				 // ``LaplaceProblem'' class as in the
				 // previous example. The only
				 // difference is that we have now
				 // declared it as a class with a
				 // template parameter, and the
				 // template parameter is of course
				 // the spatial dimension in which we
				 // would like to solve the Laplace
				 // equation. Of course, several of
				 // the member variables depend on
				 // this dimension as well, in
				 // particular the Triangulation
				 // class, which has to represent
				 // quadrilaterals or hexahedra,
				 // respectively. Apart from this,
				 // everything is as before.
template <int dim>
class LaplaceProblem 
{
  public:
    LaplaceProblem ();
    void run ();
    
  private:
    void make_grid_and_dofs ();
    void assemble_system ();
    void solve ();
    void output_results () const;

    Triangulation<dim>   triangulation;
    FE_Q<dim>            fe;
    DoFHandler<dim>      dof_handler;

    SparsityPattern      sparsity_pattern;
    SparseMatrix<double> system_matrix;

    Vector<double>       solution;
    Vector<double>       system_rhs;
};


                                 // @sect3{Right hand side and boundary values}

				 // In the following, we declare two more
				 // classes denoting the right hand side and
				 // the non-homogeneous Dirichlet boundary
				 // values. Both are functions of a
				 // dim-dimensional space variable, so we
				 // declare them as templates as well.
				 //
				 // Each of these classes is derived
				 // from a common, abstract base class
				 // Function, which declares the
				 // common interface which all
				 // functions have to follow. In
				 // particular, concrete classes have
				 // to overload the `value' function,
				 // which takes a point in
				 // dim-dimensional space as
				 // parameters and shall return the
				 // value at that point as a `double'
				 // variable.
				 //
				 // The `value' function takes a second
				 // argument, which we have here named
				 // `component': This is only meant for vector
				 // valued functions, where you may want to
				 // access a certain component of the vector
				 // at the point `p'. However, our functions
				 // are scalar, so we need not worry about
				 // this parameter and we will not use it in
				 // the implementation of the
				 // functions. Inside the library's header
				 // files, the Function base class's
				 // declaration of the `value' function has a
				 // default value of zero for the component,
				 // so we will access the `value' function of
				 // the right hand side with only one
				 // parameter, namely the point where we want
				 // to evaluate the function. A value for the
				 // component can then simply be omitted for
				 // scalar functions.
				 //
				 // Note that the C++ language forces
				 // us to declare and define a
				 // constructor to the following
				 // classes even though they are
				 // empty. This is due to the fact
				 // that the base class has no default
				 // constructor (i.e. one without
				 // arguments), even though it has a
				 // constructor which has default
				 // values for all arguments.
template <int dim>
class RightHandSide : public Function<dim> 
{
  public:
    RightHandSide () : Function<dim>() {};
    
    virtual double value (const Point<dim>   &p,
			  const unsigned int  component = 0) const;
};



template <int dim>
class BoundaryValues : public Function<dim> 
{
  public:
    BoundaryValues () : Function<dim>() {};
    
    virtual double value (const Point<dim>   &p,
			  const unsigned int  component = 0) const;
};




				 // For this example, we choose as right hand
				 // side function to function 4*(x^4+y^4) in
				 // 2D, or 4*(x^4+y^4+z^4) in 3D. We could
				 // write this distinction using an
				 // if-statement on the space dimension, but
				 // here is a simple way that also allows us
				 // to use the same function in 1D (or in 4D,
				 // if you should desire to do so), by using a
				 // short loop.  Fortunately, the compiler
				 // knows the size of the loop at compile time
				 // (remember that at the time when you define
				 // the template, the compiler doesn't know
				 // the value of ``dim'', but when it later
				 // encounters a statement or declaration
				 // ``RightHandSide<2>'', it will take the
				 // template, replace all occurrences of dim
				 // by 2 and compile the resulting function);
				 // in other words, at the time of compiling
				 // this function, the number of times the
				 // body will be executed is known, and the
				 // compiler can optimize away the overhead
				 // needed for the loop and the result will be
				 // as fast as if we had used the formulas
				 // above right away.
				 //
				 // The last thing to note is that a
				 // ``Point<dim>'' denotes a point in
				 // dim-dimensionsal space, and its individual
				 // components (i.e. `x', `y',
				 // ... coordinates) can be accessed using the
				 // () operator (in fact, the [] operator will
				 // work just as well) with indices starting
				 // at zero as usual in C and C++.
template <int dim>
double RightHandSide<dim>::value (const Point<dim> &p,
				  const unsigned int /*component*/) const 
{
  double return_value = 0;
  for (unsigned int i=0; i<dim; ++i)
    return_value += 4*std::pow(p(i), 4);

  return return_value;
}


				 // As boundary values, we choose x*x+y*y in
				 // 2D, and x*x+y*y+z*z in 3D. This happens to
				 // be equal to the square of the vector from
				 // the origin to the point at which we would
				 // like to evaluate the function,
				 // irrespective of the dimension. So that is
				 // what we return:
template <int dim>
double BoundaryValues<dim>::value (const Point<dim> &p,
				   const unsigned int /*component*/) const 
{
  return p.square();
}



                                 // @sect3{Implementation of the ``LaplaceProblem'' class}

                                 // Next for the implementation of the class
                                 // template that makes use of the functions
                                 // above. As before, we will write everything
                                 // as templates that have a formal parameter
                                 // ``dim'' that we assume unknown at the time
                                 // we define the template functions. Only
                                 // later, the compiler will find a
                                 // declaration of ``LaplaceProblem<2>'' (in
                                 // the ``main'' function, actually) and
                                 // compile the entire class with ``dim''
                                 // replaced by 2, a process referred to as
                                 // `instantiation of a template'. When doing
                                 // so, it will also replace instances of
                                 // ``RightHandSide<dim>'' by
                                 // ``RightHandSide<2>'' and instantiate the
                                 // latter class from the class template.
                                 //
                                 // In fact, the compiler will also find a
                                 // declaration ``LaplaceProblem<3>'' in
                                 // ``main()''. This will cause it to again go
                                 // back to the general
                                 // ``LaplaceProblem<dim>'' template, replace
                                 // all occurrences of ``dim'', this time by
                                 // 3, and compile the class a second
                                 // time. Note that the two instantiations
                                 // ``LaplaceProblem<2>'' and
                                 // ``LaplaceProblem<3>'' are completely
                                 // independent classes; their only common
                                 // feature is that they are both instantiated
                                 // from the same general template, but they
                                 // are not convertible into each other, for
                                 // example, and share no code (both
                                 // instantiations are compiled completely
                                 // independently).


                                 // @sect4{LaplaceProblem::LaplaceProblem}

				 // After this introduction, here is the
				 // constructor of the ``LaplaceProblem''
				 // class. It specifies the desired polynomial
				 // degree of the finite elements and
				 // associates the DoFHandler to the
				 // triangulation just as in the previous
				 // example program, step-3:
template <int dim>
LaplaceProblem<dim>::LaplaceProblem () :
                fe (1),
		dof_handler (triangulation)
{}


                                 // @sect4{LaplaceProblem::make_grid_and_dofs}

				 // Grid creation is something
				 // inherently dimension
				 // dependent. However, as long as the
				 // domains are sufficiently similar
				 // in 2D or 3D, the library can
				 // abstract for you. In our case, we
				 // would like to again solve on the
				 // square [-1,1]x[-1,1] in 2D, or on
				 // the cube [-1,1]x[-1,1]x[-1,1] in
				 // 3D; both can be termed
				 // ``hyper_cube'', so we may use the
				 // same function in whatever
				 // dimension we are. Of course, the
				 // functions that create a hypercube
				 // in two and three dimensions are
				 // very much different, but that is
				 // something you need not care
				 // about. Let the library handle the
				 // difficult things.
				 //
				 // Likewise, associating a degree of freedom
				 // with each vertex is something which
				 // certainly looks different in 2D and 3D,
				 // but that does not need to bother you
				 // either. This function therefore looks
				 // exactly like in the previous example,
				 // although it performs actions that in their
				 // details are quite different if ``dim''
				 // happens to be 3. The only significant
				 // difference from a user's perspective is
				 // the number of cells resulting, which is
				 // much higher in three than in two space
				 // dimensions!
template <int dim>
void LaplaceProblem<dim>::make_grid_and_dofs ()
{
  GridGenerator::hyper_cube (triangulation, -1, 1);
  triangulation.refine_global (4);
  
  std::cout << "   Number of active cells: "
	    << triangulation.n_active_cells()
	    << std::endl
	    << "   Total number of cells: "
	    << triangulation.n_cells()
	    << std::endl;

  dof_handler.distribute_dofs (fe);

  std::cout << "   Number of degrees of freedom: "
	    << dof_handler.n_dofs()
	    << std::endl;

  sparsity_pattern.reinit (dof_handler.n_dofs(),
			   dof_handler.n_dofs(),
			   dof_handler.max_couplings_between_dofs());
  DoFTools::make_sparsity_pattern (dof_handler, sparsity_pattern);
  sparsity_pattern.compress();

  system_matrix.reinit (sparsity_pattern);

  solution.reinit (dof_handler.n_dofs());
  system_rhs.reinit (dof_handler.n_dofs());
}


                                 // @sect4{LaplaceProblem::assemble_system}

				 // Unlike in the previous example, we
				 // would now like to use a
				 // non-constant right hand side
				 // function and non-zero boundary
				 // values. Both are tasks that are
				 // readily achieved with a only a few
				 // new lines of code in the
				 // assemblage of the matrix and right
				 // hand side.
				 //
				 // More interesting, though, is the
				 // way we assemble matrix and right
				 // hand side vector dimension
				 // independently: there is simply no
				 // difference to the 
				 // two-dimensional case. Since the
				 // important objects used in this
				 // function (quadrature formula,
				 // FEValues) depend on the dimension
				 // by way of a template parameter as
				 // well, they can take care of
				 // setting up properly everything for
				 // the dimension for which this
				 // function is compiled. By declaring
				 // all classes which might depend on
				 // the dimension using a template
				 // parameter, the library can make
				 // nearly all work for you and you
				 // don't have to care about most
				 // things.
template <int dim>
void LaplaceProblem<dim>::assemble_system () 
{  
  QGauss<dim>  quadrature_formula(2);

				   // We wanted to have a non-constant right
				   // hand side, so we use an object of the
				   // class declared above to generate the
				   // necessary data. Since this right hand
				   // side object is only used locally in the
				   // present function, we declare it here as
				   // a local variable:
  const RightHandSide<dim> right_hand_side;

				   // Compared to the previous example, in
				   // order to evaluate the non-constant right
				   // hand side function we now also need the
				   // quadrature points on the cell we are
				   // presently on (previously, we only
				   // required values and gradients of the
				   // shape function from the ``FEValues''
				   // object, as well as the quadrature
				   // weights, ``JxW''). We can tell the
				   // ``FEValues'' object to do for us by also
				   // giving it the ``update_q_points'' flag:
  FEValues<dim> fe_values (fe, quadrature_formula, 
			   update_values   | update_gradients |
                           update_q_points | update_JxW_values);

				   // We then again define a few
				   // abbreviations. The values of these
				   // variables of course depend on the
				   // dimension which we are presently
				   // using. However, the FE and Quadrature
				   // classes do all the necessary work for
				   // you and you don't have to care about the
				   // dimension dependent parts:
  const unsigned int   dofs_per_cell = fe.dofs_per_cell;
  const unsigned int   n_q_points    = quadrature_formula.n_quadrature_points;

  FullMatrix<double>   cell_matrix (dofs_per_cell, dofs_per_cell);
  Vector<double>       cell_rhs (dofs_per_cell);

  std::vector<unsigned int> local_dof_indices (dofs_per_cell);

                                   // Next, we again have to loop over all
				   // cells and assemble local contributions.
				   // Note, that a cell is a quadrilateral in
				   // two space dimensions, but a hexahedron
				   // in 3D. In fact, the
				   // ``active_cell_iterator'' data type is
				   // something different, depending on the
				   // dimension we are in, but to the outside
				   // world they look alike and you will
				   // probably never see a difference although
				   // the classes that this typedef stands for
				   // are in fact completelye unrelated:
  typename DoFHandler<dim>::active_cell_iterator cell = dof_handler.begin_active(),
						 endc = dof_handler.end();
  for (; cell!=endc; ++cell)
    {
      fe_values.reinit (cell);
      cell_matrix = 0;
      cell_rhs = 0;

				       // Now we have to assemble the
				       // local matrix and right hand
				       // side. This is done exactly
				       // like in the previous
				       // example, but now we revert
				       // the order of the loops
				       // (which we can safely do
				       // since they are independent
				       // of each other) and merge the
				       // loops for the local matrix
				       // and the local vector as far
				       // as possible to make
				       // things a bit faster.
                                       //
                                       // Assembling the right hand side
                                       // presents the only significant
                                       // difference to how we did things in
                                       // step-3: Instead of using a constant
                                       // right hand side with value 1, we use
                                       // the object representing the right
                                       // hand side and evaluate it at the
                                       // quadrature points:
      for (unsigned int q_point=0; q_point<n_q_points; ++q_point)
	for (unsigned int i=0; i<dofs_per_cell; ++i)
	  {
	    for (unsigned int j=0; j<dofs_per_cell; ++j)
	      cell_matrix(i,j) += (fe_values.shape_grad (i, q_point) *
				   fe_values.shape_grad (j, q_point) *
				   fe_values.JxW (q_point));

	    cell_rhs(i) += (fe_values.shape_value (i, q_point) *
			    right_hand_side.value (fe_values.quadrature_point (q_point)) *
			    fe_values.JxW (q_point));
	  }
                                       // As a final remark to these loops:
                                       // when we assemble the local
                                       // contributions, we have to multiply
                                       // the gradients of shape functions i
                                       // and j at point q_point and multiply
                                       // it with the scalar weights JxW. This
                                       // is actually what happens:
                                       // ``fe_values.shape_grad(i,q_point)''
                                       // returns a ``dim'' dimensional
                                       // vector, represented by a
                                       // ``Tensor<1,dim>'' object, and the
                                       // operator* that multiplies it with
                                       // the result of
                                       // ``fe_values.shape_grad(j,q_point)''
                                       // makes sure that the ``dim''
                                       // components of the two vectors are
                                       // properly contracted, and the result
                                       // is a scalar floating point number
                                       // that then is multiplied with the
                                       // weights. Internally, this operator*
                                       // makes sure that this happens
                                       // correctly for all ``dim'' components
                                       // of the vectors, whether ``dim'' be
                                       // 2, 3, or any other space dimension;
                                       // from a user's perspective, this is
                                       // not something worth bothering with,
                                       // however, making things a lot simpler
                                       // if one wants to write code dimension
                                       // independently.
      
				       // With the local systems assembled,
				       // the transfer into the global matrix
				       // and right hand side is done exactly
				       // as before, but here we have again
				       // merged some loops for efficiency:
      cell->get_dof_indices (local_dof_indices);
      for (unsigned int i=0; i<dofs_per_cell; ++i)
	{
	  for (unsigned int j=0; j<dofs_per_cell; ++j)
	    system_matrix.add (local_dof_indices[i],
			       local_dof_indices[j],
			       cell_matrix(i,j));
	  
	  system_rhs(local_dof_indices[i]) += cell_rhs(i);
	}
    }

  
				   // As the final step in this function, we
				   // wanted to have non-homogeneous boundary
				   // values in this example, contrary to the
				   // one before. This is a simple task, we
				   // only have to replace the
				   // ``ZeroFunction'' used there by an object
				   // of the class which describes the
				   // boundary values we would like to use
				   // (i.e. the ``BoundaryValues'' class
				   // declared above):
  std::map<unsigned int,double> boundary_values;
  VectorTools::interpolate_boundary_values (dof_handler,
					    0,
					    BoundaryValues<dim>(),
					    boundary_values);
  MatrixTools::apply_boundary_values (boundary_values,
				      system_matrix,
				      solution,
				      system_rhs);
}


                                 // @sect4{LaplaceProblem::solve}

				 // Solving the linear system of
				 // equations is something that looks
				 // almost identical in most
				 // programs. In particular, it is
				 // dimension independent, so this
				 // function is copied verbatim from the
				 // previous example.
template <int dim>
void LaplaceProblem<dim>::solve () 
{
  SolverControl           solver_control (1000, 1e-12);
  SolverCG<>              cg (solver_control);
  cg.solve (system_matrix, solution, system_rhs,
	    PreconditionIdentity());

				   // We have made one addition,
				   // though: since we suppress output
				   // from the linear solvers, we have
				   // to print the number of
				   // iterations by hand.
  std::cout << "   " << solver_control.last_step()
	    << " CG iterations needed to obtain convergence."
	    << std::endl;
}


                                 // @sect4{LaplaceProblem::output_results}

				 // This function also does what the
				 // respective one did in step-3. No changes
				 // here for dimension independence either.
                                 //
                                 // The only difference to the previous
                                 // example is that we want to write output in
                                 // GMV format, rather than for gnuplot (GMV
                                 // is another graphics program that, contrary
                                 // to gnuplot, shows data in nice colors,
                                 // allows rotation of geometries with the
                                 // mouse, and generates reasonable
                                 // representations of 3d data; for ways to
                                 // obtain it see the ReadMe file of
                                 // deal.II). To write data in this format, we
                                 // simply replace the
                                 // ``data_out.write_gnuplot'' call by
                                 // ``data_out.write_gmv''.
                                 //
                                 // Since the program will run both 2d and 3d
                                 // versions of the laplace solver, we use the
                                 // dimension in the filename to generate
                                 // distinct filenames for each run (in a
                                 // better program, one would check whether
                                 // `dim' can have other values than 2 or 3,
                                 // but we neglect this here for the sake of
                                 // brevity).
template <int dim>
void LaplaceProblem<dim>::output_results () const
{
  DataOut<dim> data_out;

  data_out.attach_dof_handler (dof_handler);
  data_out.add_data_vector (solution, "solution");

  data_out.build_patches ();

  std::ofstream output (dim == 2 ?
			"solution-2d.gmv" :
			"solution-3d.gmv");
  data_out.write_gmv (output);
}



                                 // @sect4{LaplaceProblem::run}

                                 // This is the function which has the
				 // top-level control over
				 // everything. Apart from one line of
				 // additional output, it is the same
				 // as for the previous example.
template <int dim>
void LaplaceProblem<dim>::run () 
{
  std::cout << "Solving problem in " << dim << " space dimensions." << std::endl;
  
  make_grid_and_dofs();
  assemble_system ();
  solve ();
  output_results ();
}


                                 // @sect3{The ``main'' function}

				 // And this is the main function. It also
				 // looks mostly like in step-3, but if you
				 // look at the code below, note how we first
				 // create a variable of type
				 // ``LaplaceProblem<2>'' (forcing the
				 // compiler to compile the class template
				 // with ``dim'' replaced by ``2'') and run a
				 // 2d simulation, and then we do the whole
				 // thing over in 3d.
				 //
				 // In practice, this is probably not what you
				 // would do very frequently (you probably
				 // either want to solve a 2d problem, or one
				 // in 3d, but not both at the same
				 // time). However, it demonstrates the
				 // mechanism by which we can simply change
				 // which dimension we want in a single place,
				 // and thereby force the compiler to
				 // recompile the dimension independent class
				 // templates for the dimension we
				 // request. The emphasis here lies on the
				 // fact that we only need to change a single
				 // place. This makes it rather trivial to
				 // debug the program in 2d where computations
				 // are fast, and then switch a single place
				 // to a 3 to run the much more computing
				 // intensive program in 3d for `real'
				 // computations.
				 //
				 // Each of the two blocks is enclosed in
				 // braces to make sure that the
				 // ``laplace_problem_2d'' variable goes out
				 // of scope (and releases the memory it
				 // holds) before we move on to allocate
				 // memory for the 3d case. Without the
				 // additional braces, the
				 // ``laplace_problem_2d'' variable would only
				 // be destroyed at the end of the function,
				 // i.e. after running the 3d problem, and
				 // would needlessly hog memory while the 3d
				 // run could actually use it.
                                 //
                                 // Finally, the first line of the function is
                                 // used to suppress some output.  Remember
                                 // that in the previous example, we had the
                                 // output from the linear solvers about the
                                 // starting residual and the number of the
                                 // iteration where convergence was
                                 // detected. This can be suppressed through
                                 // the ``deallog.depth_console(0)'' call.
                                 //
                                 // The rationale here is the following: the
                                 // deallog (i.e. deal-log, not de-allog)
                                 // variable represents a stream to which some
                                 // parts of the library write output. It
                                 // redirects this output to the console and
                                 // if required to a file. The output is
                                 // nested in a way so that each function can
                                 // use a prefix string (separated by colons)
                                 // for each line of output; if it calls
                                 // another function, that may also use its
                                 // prefix which is then printed after the one
                                 // of the calling function. Since output from
                                 // functions which are nested deep below is
                                 // usually not as important as top-level
                                 // output, you can give the deallog variable
                                 // a maximal depth of nested output for
                                 // output to console and file. The depth zero
                                 // which we gave here means that no output is
                                 // written. By changing it you can get more
                                 // information about the innards of the
                                 // library.
int main () 
{
  deallog.depth_console (0);
  {
    LaplaceProblem<2> laplace_problem_2d;
    laplace_problem_2d.run ();
  }
  
  {
    LaplaceProblem<3> laplace_problem_3d;
    laplace_problem_3d.run ();
  }
  
  return 0;
}
