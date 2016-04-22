#ifndef ACTS_MAKE_PROJECTION_MATRIX_H
#define ACTS_MAKE_PROJECTION_MATRIX_H 1

// ACTS include(s)
#include "ACTS/Utilities/AlgebraDefinitions.h"

namespace Acts
{
  /// @cond detail
  namespace detail
  {
    /**
     * @brief  initialize projection matrices
     *
     * This struct provides an initialization method for a projection matrix M such that only
     * the entries with the given indices are selected from a full parameter vector. That means,
     * M is a mapping M: (Nx1) --> (Sx1) if N is the total number of parameters and S is the
     * number of given indices.
     *
     * @tparam columns number of columns (= dimension of full parameter space)
     * @tparam rows template parameter pack containing the indices of the parameters to be projected
     *
     * @return `make_projection_matrix<columns,rows...>::init()` returns a matrix with dimensions
     *         (`sizeof...(rows)` x columns)
     */
    template<unsigned int columns, unsigned int... rows>
    struct make_projection_matrix;

    /// @cond
    // build projection matrix by iteratively stacking row vectors
    template<unsigned int columns, unsigned int i, unsigned int ... N>
    struct make_projection_matrix<columns, i, N...>
    {
      static ActsMatrixD<sizeof...(N) + 1, columns> init()
      {
        ActsRowVectorD < columns > v;
        v.setZero();
        v(i) = 1;

        ActsMatrixD < sizeof...(N) + 1, columns > m;
        m.row(0) << v;
        m.block(1, 0, sizeof...(N), columns) << make_projection_matrix<columns, N...>::init();

        return m;
      }
    };

    // projection matrix for a single local parameter is a simple row vector
    template<unsigned int columns, unsigned int i>
    struct make_projection_matrix<columns, i>
    {
      static ActsRowVectorD<columns> init()
      {
        ActsRowVectorD<columns> v;
        v.setZero();
        v(i) = 1;
        return v;
      }
    };
    /// @endcond
  }  // end of namespace detail
  /// @endcond
}  // end of namespace Acts

#endif // ACTS_MAKE_PROJECTION_MATRIX_H
