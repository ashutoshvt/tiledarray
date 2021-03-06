/*
 *  This file is a part of TiledArray.
 *  Copyright (C) 2018  Virginia Tech
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Chong Peng
 *  Department of Chemistry, Virginia Tech
 *
 *  expressions_sparse.cpp
 *  May 4, 2018
 *
 */

#include "range_fixture.h"
#include "tiledarray.h"
#include "unit_test_config.h"

using namespace TiledArray;

struct ExpressionsSparseFixture : public TiledRangeFixture {
  ExpressionsSparseFixture()
      : s_tr_1(make_random_sparseshape(tr)),
        s_tr_2(make_random_sparseshape(tr)),
        s_tr1_1(make_random_sparseshape(trange1)),
        s_tr1_2(make_random_sparseshape(trange1)),
        s_tr2(make_random_sparseshape(trange2)),
        a(*GlobalFixture::world, tr, s_tr_1),
        b(*GlobalFixture::world, tr, s_tr_2),
        u(*GlobalFixture::world, trange1, s_tr1_1),
        v(*GlobalFixture::world, trange1, s_tr1_2) {
    random_fill(a);
    random_fill(b);
    random_fill(u);
    random_fill(v);
    GlobalFixture::world->gop.fence();
    a.truncate();
    b.truncate();
    u.truncate();
    v.truncate();
  }

  template <typename Tile, typename Policy>
  static void random_fill(DistArray<Tile, Policy>& array) {
    typename DistArray<Tile, Policy>::pmap_interface::const_iterator it =
        array.pmap()->begin();
    typename DistArray<Tile, Policy>::pmap_interface::const_iterator end =
        array.pmap()->end();
    for (; it != end; ++it) {
      if (!array.is_zero(*it))
        array.set(*it, array.world().taskq.add(
                           &ExpressionsSparseFixture::template make_rand_tile<
                               DistArray<Tile> >,
                           array.trange().make_tile_range(*it)));
    }
  }

  template <typename T>
  static void set_random(T& t) {
    t = GlobalFixture::world->rand() % 101;
  }

  template <typename T>
  static void set_random(std::complex<T>& t) {
    t = std::complex<T>{T(GlobalFixture::world->rand() % 101),
                        T(GlobalFixture::world->rand() % 101)};
  }

  // Fill a tile with random data
  template <typename A>
  static typename A::value_type make_rand_tile(
      const typename A::value_type::range_type& r) {
    typename A::value_type tile(r);
    for (std::size_t i = 0ul; i < tile.size(); ++i) set_random(tile[i]);
    return tile;
  }

  // make a tile with 0 data
  template <typename A>
  static typename A::value_type make_zero_tile(
      const typename A::value_type::range_type& r) {
    typename A::value_type tile(r, 0);
    return tile;
  }

  template <typename M, typename A>
  static void rand_fill_matrix_and_array(M& matrix, A& array, int seed = 42) {
    TA_ASSERT(std::size_t(matrix.size()) ==
              array.trange().elements_range().volume());
    matrix.fill(0);

    GlobalFixture::world->srand(seed);

    // Iterate over local tiles
    for (typename A::iterator it = array.begin(); it != array.end(); ++it) {
      typename A::value_type tile(array.trange().make_tile_range(it.index()));
      for (Range::const_iterator rit = tile.range().begin();
           rit != tile.range().end(); ++rit) {
        const std::size_t elem_index = array.elements_range().ordinal(*rit);
        tile[*rit] =
            (matrix.array()(elem_index) = (GlobalFixture::world->rand() % 101));
      }
      *it = tile;
    }
    GlobalFixture::world->gop.sum(&matrix(0, 0), matrix.size());
  }

  template <typename Tile, typename Policy>
  Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic> make_matrix(
      DistArray<Tile, Policy>& array) {
    // Check that the array will fit in a matrix or vector

    // Construct the Eigen matrix
    Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic> matrix =
        Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic>::Zero(
            array.trange().elements_range().extent(0),
            (array.trange().tiles_range().rank() == 2
                 ? array.trange().elements_range().extent(1)
                 : 1));

    // Spawn tasks to copy array tiles to the Eigen matrix
    for (std::size_t i = 0; i < array.size(); ++i) {
      if (!array.is_zero(i))
        tensor_to_eigen_submatrix(array.find(i).get(), matrix);
    }

    return matrix;
  }

  /// make a shape with approximate half dense and half sparse
  static SparseShape<float> make_random_sparseshape(const TiledRange& tr) {
    std::size_t n = tr.tiles_range().volume();
    Tensor<float> norms(tr.tiles_range(), 0.0);

    // make sure all mpi gets the same shape
    if(GlobalFixture::world->rank() == 0){
      for (std::size_t i = 0; i < n; i++) {
        norms[i] = GlobalFixture::world->drand() > 0.5 ? 0.0 : 1.0;
      }
    }

    GlobalFixture::world->gop.broadcast_serializable(norms, 0);

    return SparseShape<float>(norms, tr);
  }

  ~ExpressionsSparseFixture() { GlobalFixture::world->gop.fence(); }

  const static TiledRange trange1;
  const static TiledRange trange2;
  SparseShape<float> s_tr_1;
  SparseShape<float> s_tr_2;
  SparseShape<float> s_tr1_1;
  SparseShape<float> s_tr1_2;
  SparseShape<float> s_tr2;
  TSpArrayI a;
  TSpArrayI b;
  TSpArrayI c;
  TSpArrayI u;
  TSpArrayI v;
  TSpArrayI w;
};  // ExpressionsFixture

// Instantiate static variables for fixture
const TiledRange ExpressionsSparseFixture::trange1 = {
    {0, 2, 5, 10, 17, 28, 41}};
const TiledRange ExpressionsSparseFixture::trange2 = {
    {0, 2, 5, 10, 17, 28, 41}, {0, 3, 6, 11, 18, 29, 42}};

BOOST_FIXTURE_TEST_SUITE(expressions_sparse_suite, ExpressionsSparseFixture)

BOOST_AUTO_TEST_CASE(tensor_factories) {
  const auto& ca = a;
  const std::array<int, 3> lobound{{3, 3, 3}};
  const std::array<int, 3> upbound{{5, 5, 5}};

  BOOST_CHECK_NO_THROW(c("a,b,c") = a("c,b,a"));
  BOOST_CHECK_NO_THROW(c("a,b,c") += a("c,b,a"));
  BOOST_CHECK_NO_THROW(c("a,b,c") = c("a,c,b") + a("c,b,a"));
  BOOST_CHECK_NO_THROW(c("a,b,c") -= a("c,b,a"));
  BOOST_CHECK_NO_THROW(c("a,b,c") = c("a,c,b") - a("c,b,a"));
  BOOST_CHECK_NO_THROW(c("a,b,c") *= a("c,b,a"));
  BOOST_CHECK_NO_THROW(c("a,b,c") = c("a,c,b") * a("c,b,a"));
  BOOST_CHECK_NO_THROW(c("a,b,c") = a("c,b,a").conj());
  BOOST_CHECK_NO_THROW(c("a,b,c") = a("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(c("a,b,c") = a("a,b,c").block({3, 3, 3}, {5, 5, 5}));
  BOOST_CHECK_NO_THROW(c("a,b,c") = ca("c,b,a"));
  BOOST_CHECK_NO_THROW(c("a,b,c") = ca("c,b,a").conj());
  BOOST_CHECK_NO_THROW(c("a,b,c") = ca("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(c("a,b,c") = ca("a,b,c").block({3, 3, 3}, {5, 5, 5}));
}

BOOST_AUTO_TEST_CASE(block_tensor_factories) {
  const auto& ca = a;
  const std::array<int, 3> lobound{{3, 3, 3}};
  const std::array<int, 3> upbound{{5, 5, 5}};

  BOOST_CHECK_NO_THROW(c("a,b,c") =
                           a("a,b,c").block({3, 3, 3}, {5, 5, 5}).conj());
  BOOST_CHECK_NO_THROW(c("a,b,c") = a("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(c("a,b,c") += a("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(c("a,b,c") =
                           c("b,a,c") + a("b,a,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(c("a,b,c") -= a("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(c("a,b,c") =
                           c("b,a,c") - a("b,a,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(c("a,b,c") *= a("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(c("a,b,c") =
                           c("b,a,c") * a("b,a,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(c("a,b,c") = a("a,b,c").block(lobound, upbound).conj());
  BOOST_CHECK_NO_THROW(c("a,b,c") = ca("a,b,c").block(lobound, upbound).conj());

  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * a("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(c("a,b,c") = a("a,b,c").block(lobound, upbound) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") =
                           2 * (2 * a("a,b,c").block(lobound, upbound)));
  BOOST_CHECK_NO_THROW(c("a,b,c") =
                           (2 * a("a,b,c").block(lobound, upbound)) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = -a("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -(2 * a("a,b,c").block(lobound, upbound)));

  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(a("a,b,c").block(lobound, upbound)));
  BOOST_CHECK_NO_THROW(c("a,b,c") =
                           conj(conj(a("a,b,c").block(lobound, upbound))));
  BOOST_CHECK_NO_THROW(c("a,b,c") =
                           conj(2 * a("a,b,c").block(lobound, upbound)));
  BOOST_CHECK_NO_THROW(c("a,b,c") =
                           conj(conj(2 * a("a,b,c").block(lobound, upbound))));

  BOOST_CHECK_NO_THROW(c("a,b,c") =
                           2 * conj(a("a,b,c").block(lobound, upbound)));
  BOOST_CHECK_NO_THROW(c("a,b,c") =
                           conj(a("a,b,c").block(lobound, upbound)) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") =
                           2 * conj(2 * a("a,b,c").block(lobound, upbound)));
  BOOST_CHECK_NO_THROW(c("a,b,c") =
                           conj(2 * a("a,b,c").block(lobound, upbound)) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = -conj(a("a,b,c").block(lobound, upbound)));
  BOOST_CHECK_NO_THROW(c("a,b,c") =
                           -conj(2 * a("a,b,c").block(lobound, upbound)));
}

BOOST_AUTO_TEST_CASE(scaled_tensor_factories) {
  BOOST_CHECK_NO_THROW(c("a,b,c") = a("c,b,a") * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * a("c,b,a"));
  BOOST_CHECK_NO_THROW(c("a,b,c") = (2 * a("c,b,a")) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * (2 * a("c,b,a")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -a("c,b,a"));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -(2 * a("c,b,a")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(a("c,b,a")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(conj(a("c,b,a"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(2 * a("c,b,a")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(conj(2 * a("c,b,a"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(a("c,b,a")) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * conj(a("c,b,a")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(2 * a("c,b,a")) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * conj(2 * a("c,b,a")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -conj(a("c,b,a")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -conj(2 * a("c,b,a")));
}

BOOST_AUTO_TEST_CASE(add_factories) {
  BOOST_CHECK_NO_THROW(c("a,b,c") = a("c,b,a") + b("a,b,c"));
  BOOST_CHECK_NO_THROW(c("a,b,c") = (a("c,b,a") + b("a,b,c")) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * (a("c,b,a") + b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = (2 * (a("c,b,a") + b("a,b,c"))) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * (2 * (a("c,b,a") + b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -(a("c,b,a") + b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -(2 * (a("c,b,a") + b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(a("c,b,a") + b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(conj(a("c,b,a") + b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(2 * (a("c,b,a") + b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(conj(2 * (a("c,b,a") + b("a,b,c")))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(2 * (conj(a("c,b,a") + b("a,b,c")))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(a("c,b,a") + b("a,b,c")) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * conj(a("c,b,a") + b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(2 * (a("c,b,a") + b("a,b,c"))) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * conj(2 * (a("c,b,a") + b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -conj(a("c,b,a") + b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -conj(2 * (a("c,b,a") + b("a,b,c"))) * 2);
}

BOOST_AUTO_TEST_CASE(subt_factories) {
  BOOST_CHECK_NO_THROW(c("a,b,c") = a("c,b,a") - b("a,b,c"));
  BOOST_CHECK_NO_THROW(c("a,b,c") = (a("c,b,a") - b("a,b,c")) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * (a("c,b,a") - b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = (2 * (a("c,b,a") - b("a,b,c"))) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * (2 * (a("c,b,a") - b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -(a("c,b,a") - b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -(2 * (a("c,b,a") - b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(a("c,b,a") - b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(conj(a("c,b,a") - b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(2 * (a("c,b,a") - b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(conj(2 * (a("c,b,a") - b("a,b,c")))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(2 * (conj(a("c,b,a") - b("a,b,c")))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(a("c,b,a") - b("a,b,c")) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * conj(a("c,b,a") - b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(2 * (a("c,b,a") - b("a,b,c"))) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * conj(2 * (a("c,b,a") - b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -conj(a("c,b,a") - b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -conj(2 * (a("c,b,a") - b("a,b,c"))) * 2);
}

BOOST_AUTO_TEST_CASE(mult_factories) {
  BOOST_CHECK_NO_THROW(c("a,b,c") = a("c,b,a") * b("a,b,c"));
  BOOST_CHECK_NO_THROW(c("a,b,c") = (a("c,b,a") * b("a,b,c")) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * (a("c,b,a") * b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = (2 * (a("c,b,a") * b("a,b,c"))) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * (2 * (a("c,b,a") * b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -(a("c,b,a") * b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -(2 * (a("c,b,a") * b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(a("c,b,a") * b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(conj(a("c,b,a") * b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(2 * (a("c,b,a") * b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(conj(2 * (a("c,b,a") * b("a,b,c")))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(2 * (conj(a("c,b,a") * b("a,b,c")))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(a("c,b,a") * b("a,b,c")) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * conj(a("c,b,a") * b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = conj(2 * (a("c,b,a") * b("a,b,c"))) * 2);
  BOOST_CHECK_NO_THROW(c("a,b,c") = 2 * conj(2 * (a("c,b,a") * b("a,b,c"))));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -conj(a("c,b,a") * b("a,b,c")));
  BOOST_CHECK_NO_THROW(c("a,b,c") = -conj(2 * (a("c,b,a") * b("a,b,c"))) * 2);
}

BOOST_AUTO_TEST_CASE(complex_tensor_factories) {
  TSpArrayZ x(*GlobalFixture::world, tr, s_tr_1);
  TSpArrayZ z(*GlobalFixture::world, tr, s_tr_1);
  random_fill(x);
  const auto& cx = x;
  const std::array<int, 3> lobound{{3, 3, 3}};
  const std::array<int, 3> upbound{{5, 5, 5}};

  BOOST_CHECK_NO_THROW(z("a,b,c") = x("c,b,a"));
  BOOST_CHECK_NO_THROW(z("a,b,c") += x("c,b,a"));
  BOOST_CHECK_NO_THROW(z("a,b,c") -= x("c,b,a"));
  BOOST_CHECK_NO_THROW(z("a,b,c") *= x("c,b,a"));
  BOOST_CHECK_NO_THROW(z("a,b,c") = x("c,b,a").conj());
  BOOST_CHECK_NO_THROW(z("a,b,c") = x("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(z("a,b,c") = x("a,b,c").block({3, 3, 3}, {5, 5, 5}));
  BOOST_CHECK_NO_THROW(z("a,b,c") = cx("c,b,a"));
  BOOST_CHECK_NO_THROW(z("a,b,c") = cx("c,b,a").conj());
  BOOST_CHECK_NO_THROW(z("a,b,c") = cx("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(z("a,b,c") = cx("a,b,c").block({3, 3, 3}, {5, 5, 5}));
}

BOOST_AUTO_TEST_CASE(complex_block_tensor_factories) {
  TSpArrayZ x(*GlobalFixture::world, tr, s_tr_1);
  TSpArrayZ z;
  random_fill(x);
  const auto& cx = x;
  const std::array<int, 3> lobound{{3, 3, 3}};
  const std::array<int, 3> upbound{{5, 5, 5}};

  BOOST_CHECK_NO_THROW(z("a,b,c") = x("a,b,c").block({3, 3, 3}, {5, 5, 5}));
  BOOST_CHECK_NO_THROW(z("a,b,c") = x("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(z("a,b,c") = x("a,b,c").block(lobound, upbound).conj());
  BOOST_CHECK_NO_THROW(z("a,b,c") = cx("a,b,c").block({3, 3, 3}, {5, 5, 5}));
  BOOST_CHECK_NO_THROW(z("a,b,c") = cx("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(z("a,b,c") = cx("a,b,c").block(lobound, upbound).conj());
  BOOST_CHECK_NO_THROW(z("a,b,c") += x("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(z("a,b,c") -= x("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(z("a,b,c") *= x("a,b,c").block(lobound, upbound));

  BOOST_CHECK_NO_THROW(z("a,b,c") = 2.0 * x("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(z("a,b,c") = x("a,b,c").block(lobound, upbound) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           2.0 * (2.0 * x("a,b,c").block(lobound, upbound)));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           (2.0 * x("a,b,c").block(lobound, upbound)) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = -x("a,b,c").block(lobound, upbound));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           -(2.0 * x("a,b,c").block(lobound, upbound)));

  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(x("a,b,c").block(lobound, upbound)));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           conj(conj(x("a,b,c").block(lobound, upbound))));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           conj(2.0 * x("a,b,c").block(lobound, upbound)));
  BOOST_CHECK_NO_THROW(
      z("a,b,c") = conj(conj(2.0 * x("a,b,c").block(lobound, upbound))));

  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           2.0 * conj(x("a,b,c").block(lobound, upbound)));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           conj(x("a,b,c").block(lobound, upbound)) * 2.0);
  BOOST_CHECK_NO_THROW(
      z("a,b,c") = 2.0 * conj(2.0 * x("a,b,c").block(lobound, upbound)));
  BOOST_CHECK_NO_THROW(
      z("a,b,c") = conj(2.0 * x("a,b,c").block(lobound, upbound)) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = -conj(x("a,b,c").block(lobound, upbound)));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           -conj(2.0 * x("a,b,c").block(lobound, upbound)));
}

BOOST_AUTO_TEST_CASE(complex_scaled_tensor_factories) {
  TSpArrayZ x(*GlobalFixture::world, tr, s_tr_1);
  TSpArrayZ z;
  random_fill(x);

  BOOST_CHECK_NO_THROW(z("a,b,c") = x("c,b,a") * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = 2.0 * x("c,b,a"));
  BOOST_CHECK_NO_THROW(z("a,b,c") = (2.0 * x("c,b,a")) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = 2.0 * (2.0 * x("c,b,a")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = -x("c,b,a"));
  BOOST_CHECK_NO_THROW(z("a,b,c") = -(2.0 * x("c,b,a")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(x("c,b,a")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(conj(x("c,b,a"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(2.0 * x("c,b,a")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(conj(2.0 * x("c,b,a"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(x("c,b,a")) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = 2.0 * conj(x("c,b,a")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(2.0 * x("c,b,a")) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = 2.0 * conj(2.0 * x("c,b,a")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = -conj(x("c,b,a")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = -conj(2.0 * x("c,b,a")));
}

BOOST_AUTO_TEST_CASE(complex_add_factories) {
  TSpArrayZ x(*GlobalFixture::world, tr, s_tr_1);
  TSpArrayZ y(*GlobalFixture::world, tr, s_tr_2);
  TSpArrayZ z;
  random_fill(x);
  random_fill(y);

  BOOST_CHECK_NO_THROW(z("a,b,c") = x("c,b,a") + y("a,b,c"));
  BOOST_CHECK_NO_THROW(z("a,b,c") = (x("c,b,a") + y("a,b,c")) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = 2.0 * (x("c,b,a") + y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = (2.0 * (x("c,b,a") + y("a,b,c"))) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = 2.0 * (2.0 * (x("c,b,a") + y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = -(x("c,b,a") + y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = -(2.0 * (x("c,b,a") + y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(x("c,b,a") + y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(conj(x("c,b,a") + y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(2.0 * (x("c,b,a") + y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           conj(conj(2.0 * (x("c,b,a") + y("a,b,c")))));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           conj(2.0 * (conj(x("c,b,a") + y("a,b,c")))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(x("c,b,a") + y("a,b,c")) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = 2.0 * conj(x("c,b,a") + y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           conj(2.0 * (x("c,b,a") + y("a,b,c"))) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           2.0 * conj(2.0 * (x("c,b,a") + y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = -conj(x("c,b,a") + y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           -conj(2.0 * (x("c,b,a") + y("a,b,c"))) * 2.0);
}

BOOST_AUTO_TEST_CASE(complex_subt_factories) {
  TSpArrayZ x(*GlobalFixture::world, tr, s_tr_1);
  TSpArrayZ y(*GlobalFixture::world, tr, s_tr_2);
  TSpArrayZ z;
  random_fill(x);
  random_fill(y);

  BOOST_CHECK_NO_THROW(z("a,b,c") = x("c,b,a") - y("a,b,c"));
  BOOST_CHECK_NO_THROW(z("a,b,c") = (x("c,b,a") - y("a,b,c")) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = 2.0 * (x("c,b,a") - y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = (2.0 * (x("c,b,a") - y("a,b,c"))) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = 2.0 * (2.0 * (x("c,b,a") - y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = -(x("c,b,a") - y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = -(2.0 * (x("c,b,a") - y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(x("c,b,a") - y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(conj(x("c,b,a") - y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(2.0 * (x("c,b,a") - y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           conj(conj(2.0 * (x("c,b,a") - y("a,b,c")))));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           conj(2.0 * (conj(x("c,b,a") - y("a,b,c")))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(x("c,b,a") - y("a,b,c")) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = 2.0 * conj(x("c,b,a") - y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           conj(2.0 * (x("c,b,a") - y("a,b,c"))) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           2.0 * conj(2.0 * (x("c,b,a") - y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = -conj(x("c,b,a") - y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           -conj(2.0 * (x("c,b,a") - y("a,b,c"))) * 2.0);
}

BOOST_AUTO_TEST_CASE(complex_mult_factories) {
  TSpArrayZ x(*GlobalFixture::world, tr, s_tr_1);
  TSpArrayZ y(*GlobalFixture::world, tr, s_tr_2);
  TSpArrayZ z;
  random_fill(x);
  random_fill(y);

  BOOST_CHECK_NO_THROW(z("a,b,c") = x("c,b,a") * y("a,b,c"));
  BOOST_CHECK_NO_THROW(z("a,b,c") = (x("c,b,a") * y("a,b,c")) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = 2.0 * (x("c,b,a") * y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = (2.0 * (x("c,b,a") * y("a,b,c"))) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = 2.0 * (2.0 * (x("c,b,a") * y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = -(x("c,b,a") * y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = -(2.0 * (x("c,b,a") * y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(x("c,b,a") * y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(conj(x("c,b,a") * y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(2.0 * (x("c,b,a") * y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           conj(conj(2.0 * (x("c,b,a") * y("a,b,c")))));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           conj(2.0 * (conj(x("c,b,a") * y("a,b,c")))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = conj(x("c,b,a") * y("a,b,c")) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") = 2.0 * conj(x("c,b,a") * y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           conj(2.0 * (x("c,b,a") * y("a,b,c"))) * 2.0);
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           2.0 * conj(2.0 * (x("c,b,a") * y("a,b,c"))));
  BOOST_CHECK_NO_THROW(z("a,b,c") = -conj(x("c,b,a") * y("a,b,c")));
  BOOST_CHECK_NO_THROW(z("a,b,c") =
                           -conj(2.0 * (x("c,b,a") * y("a,b,c"))) * 2.0);
}

BOOST_AUTO_TEST_CASE(permute) {
  Permutation perm({2, 1, 0});
  BOOST_REQUIRE_NO_THROW(a("a,b,c") = b("c,b,a"));

  for (std::size_t i = 0ul; i < b.size(); ++i) {
    const std::size_t perm_index = a.range().ordinal(perm * b.range().idx(i));
    if (a.is_local(perm_index) && !a.is_zero(perm_index)) {
      TSpArrayI::value_type a_tile = a.find(perm_index).get();
      TSpArrayI::value_type perm_b_tile = perm * b.find(i).get();

      BOOST_CHECK_EQUAL(a_tile.range(), perm_b_tile.range());
      for (std::size_t j = 0ul; j < a_tile.size(); ++j)
        BOOST_CHECK_EQUAL(a_tile[j], perm_b_tile[j]);
    } else if (a.is_local(perm_index) && a.is_zero(perm_index)) {
      BOOST_CHECK(b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(scale_permute) {
  Permutation perm({2, 1, 0});
  BOOST_REQUIRE_NO_THROW(a("a,b,c") = 2 * b("c,b,a"));

  for (std::size_t i = 0ul; i < b.size(); ++i) {
    const std::size_t perm_index = a.range().ordinal(perm * b.range().idx(i));
    if (a.is_local(perm_index) && !a.is_zero(perm_index)) {
      TSpArrayI::value_type a_tile = a.find(perm_index).get();
      TSpArrayI::value_type perm_b_tile = perm * b.find(i).get();

      BOOST_CHECK_EQUAL(a_tile.range(), perm_b_tile.range());
      for (std::size_t j = 0ul; j < a_tile.size(); ++j)
        BOOST_CHECK_EQUAL(a_tile[j], 2 * perm_b_tile[j]);
    } else if (a.is_local(perm_index) && a.is_zero(perm_index)) {
      BOOST_CHECK(b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(block) {
  BOOST_REQUIRE_NO_THROW(c("a,b,c") = a("a,b,c").block({3, 3, 3}, {5, 5, 5}));

  BlockRange block_range(a.trange().tiles_range(), {3, 3, 3}, {5, 5, 5});

  for (std::size_t index = 0ul; index < block_range.volume(); ++index) {
    if (!a.is_zero(block_range.ordinal(index))) {
      Tensor<int> arg_tile = a.find(block_range.ordinal(index)).get();
      Tensor<int> result_tile = c.find(index).get();

      for (unsigned int r = 0u; r < arg_tile.range().rank(); ++r) {
        BOOST_CHECK_EQUAL(
            result_tile.range().lobound(r),
            arg_tile.range().lobound(r) - a.trange().data()[r].tile(3).first);

        BOOST_CHECK_EQUAL(
            result_tile.range().upbound(r),
            arg_tile.range().upbound(r) - a.trange().data()[r].tile(3).first);

        BOOST_CHECK_EQUAL(result_tile.range().extent(r),
                          arg_tile.range().extent(r));

        BOOST_CHECK_EQUAL(result_tile.range().stride(r),
                          arg_tile.range().stride(r));
      }
      BOOST_CHECK_EQUAL(result_tile.range().volume(),
                        arg_tile.range().volume());

      // Check that the data is correct for the result array.
      for (std::size_t j = 0ul; j < result_tile.range().volume(); ++j) {
        BOOST_CHECK_EQUAL(result_tile[j], arg_tile[j]);
      }
    } else {
      BOOST_CHECK(c.is_zero(index));
    }
  }
}

BOOST_AUTO_TEST_CASE(const_block) {
  const TSpArrayI& ca = a;
  BOOST_REQUIRE_NO_THROW(c("a,b,c") = ca("a,b,c").block({3, 3, 3}, {5, 5, 5}));

  BlockRange block_range(a.trange().tiles_range(), {3, 3, 3}, {5, 5, 5});

  for (std::size_t index = 0ul; index < block_range.volume(); ++index) {
    if (!a.is_zero(block_range.ordinal(index))) {
      Tensor<int> arg_tile = a.find(block_range.ordinal(index)).get();
      Tensor<int> result_tile = c.find(index).get();

      for (unsigned int r = 0u; r < arg_tile.range().rank(); ++r) {
        BOOST_CHECK_EQUAL(
            result_tile.range().lobound(r),
            arg_tile.range().lobound(r) - a.trange().data()[r].tile(3).first);

        BOOST_CHECK_EQUAL(
            result_tile.range().upbound(r),
            arg_tile.range().upbound(r) - a.trange().data()[r].tile(3).first);

        BOOST_CHECK_EQUAL(result_tile.range().extent(r),
                          arg_tile.range().extent(r));

        BOOST_CHECK_EQUAL(result_tile.range().stride(r),
                          arg_tile.range().stride(r));
      }
      BOOST_CHECK_EQUAL(result_tile.range().volume(),
                        arg_tile.range().volume());

      // Check that the data is correct for the result array.
      for (std::size_t j = 0ul; j < result_tile.range().volume(); ++j) {
        BOOST_CHECK_EQUAL(result_tile[j], arg_tile[j]);
      }
    } else {
      BOOST_CHECK(c.is_zero(index));
    }
  }
}

BOOST_AUTO_TEST_CASE(scal_block) {
  BOOST_REQUIRE_NO_THROW(c("a,b,c") =
                             2 * a("a,b,c").block({3, 3, 3}, {5, 5, 5}));

  BlockRange block_range(a.trange().tiles_range(), {3, 3, 3}, {5, 5, 5});

  for (std::size_t index = 0ul; index < block_range.volume(); ++index) {
    if (!a.is_zero(block_range.ordinal(index))) {
      Tensor<int> arg_tile = a.find(block_range.ordinal(index)).get();
      Tensor<int> result_tile = c.find(index).get();

      for (unsigned int r = 0u; r < arg_tile.range().rank(); ++r) {
        BOOST_CHECK_EQUAL(
            result_tile.range().lobound(r),
            arg_tile.range().lobound(r) - a.trange().data()[r].tile(3).first);

        BOOST_CHECK_EQUAL(
            result_tile.range().upbound(r),
            arg_tile.range().upbound(r) - a.trange().data()[r].tile(3).first);

        BOOST_CHECK_EQUAL(result_tile.range().extent(r),
                          arg_tile.range().extent(r));

        BOOST_CHECK_EQUAL(result_tile.range().stride(r),
                          arg_tile.range().stride(r));
      }
      BOOST_CHECK_EQUAL(result_tile.range().volume(),
                        arg_tile.range().volume());

      for (std::size_t j = 0ul; j < result_tile.range().volume(); ++j) {
        BOOST_CHECK_EQUAL(result_tile[j], 2 * arg_tile[j]);
      }
    } else {
      BOOST_CHECK(c.is_zero(index));
    }
  }
}

BOOST_AUTO_TEST_CASE(assign_sub_block) {
  c = TSpArrayI(*GlobalFixture::world, tr, s_tr_1);
  c.fill_local(0.0);
  c.truncate();

  BOOST_REQUIRE_NO_THROW(c("a,b,c").block({3, 3, 3}, {5, 5, 5}) =
                             2 * a("a,b,c").block({3, 3, 3}, {5, 5, 5}));

  BlockRange block_range(a.trange().tiles_range(), {3, 3, 3}, {5, 5, 5});

  for (std::size_t index = 0ul; index < block_range.volume(); ++index) {
    if (!c.is_zero(block_range.ordinal(index))) {
      Tensor<int> arg_tile = a.find(block_range.ordinal(index)).get();
      Tensor<int> result_tile = c.find(block_range.ordinal(index)).get();

      BOOST_CHECK_EQUAL(result_tile.range(), arg_tile.range());

      for (std::size_t j = 0ul; j < result_tile.range().volume(); ++j) {
        BOOST_CHECK_EQUAL(result_tile[j], 2 * arg_tile[j]);
      }
    } else {
      BOOST_CHECK(a.is_zero(block_range.ordinal(index)));
    }
  }
}
BOOST_AUTO_TEST_CASE(assign_subblock_block_contract) {
  w = TSpArrayI(*GlobalFixture::world, trange2, s_tr2);
  w.fill_local(0.0);
  w.truncate();

  BOOST_REQUIRE_NO_THROW(w("a,b").block({3, 3}, {5, 5}) =
                             a("a,c,d").block({3, 2, 3}, {5, 5, 5}) *
                             b("c,d,b").block({2, 3, 3}, {5, 5, 5}));
}

BOOST_AUTO_TEST_CASE(assign_subblock_block_permute_contract) {
  w = TSpArrayI(*GlobalFixture::world, trange2, s_tr2);
  w.fill_local(0.0);
  w.truncate();

  BOOST_REQUIRE_NO_THROW(w("a,b").block({3, 3}, {5, 5}) =
                             a("a,c,d").block({3, 2, 3}, {5, 5, 5}) *
                             b("d,c,b").block({3, 2, 3}, {5, 5, 5}));
}

BOOST_AUTO_TEST_CASE(block_contract) {
  BOOST_REQUIRE_NO_THROW(w("a,b") = a("a,c,d").block({3, 2, 3}, {5, 5, 5}) *
                                    b("c,d,b").block({2, 3, 3}, {5, 5, 5}));
}

BOOST_AUTO_TEST_CASE(block_permute_contract) {
  BOOST_REQUIRE_NO_THROW(w("a,b") = a("a,c,d").block({3, 2, 3}, {5, 5, 5}) *
                                    b("d,c,b").block({3, 2, 3}, {5, 5, 5}));
}

BOOST_AUTO_TEST_CASE(add) {
  BOOST_REQUIRE_NO_THROW(c("a,b,c") = a("a,b,c") + b("a,b,c"));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], a_tile[j] + b_tile[j]);
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = (2 * a("a,b,c")) + b("a,b,c"));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], (2 * a_tile[j]) + b_tile[j]);
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = a("a,b,c") + (3 * b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], a_tile[j] + (3 * b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = (2 * a("a,b,c")) + (3 * b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], (2 * a_tile[j]) + (3 * b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(add_to) {
  c("a,b,c") = a("a,b,c");
  BOOST_REQUIRE_NO_THROW(c("a,b,c") += b("a,b,c"));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], a_tile[j] + b_tile[j]);
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }

  c("a,b,c") = a("a,b,c");
  BOOST_REQUIRE_NO_THROW(c("a,b,c") = c("a,b,c") + b("a,b,c"));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], a_tile[j] + b_tile[j]);
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(add_permute) {
  Permutation perm({2, 1, 0});

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = (2 * a("c,b,a")) + (3 * b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    const size_t perm_index = c.range().ordinal(perm * a.range().idx(i));
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(perm_index) ? make_zero_tile<TSpArrayI>(c_tile.range())
                                : perm * a.find(perm_index).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], (2 * a_tile[j]) + (3 * b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(perm_index) && b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(scale_add) {
  BOOST_REQUIRE_NO_THROW(c("a,b,c") = 5 * (a("a,b,c") + b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5 * (a_tile[j] + b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = 5 * ((2 * a("a,b,c")) + b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5 * ((2 * a_tile[j]) + b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = 5 * (a("a,b,c") + (3 * b("a,b,c"))));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5 * (a_tile[j] + (3 * b_tile[j])));
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") =
                             5 * ((2 * a("a,b,c")) + (3 * b("a,b,c"))));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5 * ((2 * a_tile[j]) + (3 * b_tile[j])));
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE( scale_add_permute ) {
  Permutation perm({2, 1, 0});

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = 5*(2 * a("c,b,a")) + (3 * b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    const size_t perm_index = c.range().ordinal(perm * a.range().idx(i));
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
              a.is_zero(perm_index) ? make_zero_tile<TSpArrayI>(c_tile.range())
                                    : perm * a.find(perm_index).get();
      TSpArrayI::value_type b_tile =
              b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                           : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5*(2 * a_tile[j]) + (3 * b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(perm_index) && b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(subt) {
  BOOST_REQUIRE_NO_THROW(c("a,b,c") = a("a,b,c") - b("a,b,c"));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], a_tile[j] - b_tile[j]);
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = (2 * a("a,b,c")) - b("a,b,c"));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], (2 * a_tile[j]) - b_tile[j]);
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = a("a,b,c") - (3 * b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], a_tile[j] - (3 * b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = (2 * a("a,b,c")) - (3 * b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], (2 * a_tile[j]) - (3 * b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(subt_to) {
  c("a,b,c") = a("a,b,c");
  BOOST_REQUIRE_NO_THROW(c("a,b,c") -= b("a,b,c"));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], a_tile[j] - b_tile[j]);
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }

  c("a,b,c") = a("a,b,c");
  BOOST_REQUIRE_NO_THROW(c("a,b,c") = c("a,b,c") - b("a,b,c"));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
              a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                           : a.find(i).get();
      TSpArrayI::value_type b_tile =
              b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                           : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], a_tile[j] - b_tile[j]);
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(sub_permute) {
  Permutation perm({2, 1, 0});

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = (2 * a("c,b,a")) - (3 * b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    const size_t perm_index = c.range().ordinal(perm * a.range().idx(i));
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
              a.is_zero(perm_index) ? make_zero_tile<TSpArrayI>(c_tile.range())
                                    : perm * a.find(perm_index).get();
      TSpArrayI::value_type b_tile =
              b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                           : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], (2 * a_tile[j]) - (3 * b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(perm_index) && b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(scale_subt) {
  BOOST_REQUIRE_NO_THROW(c("a,b,c") = 5 * (a("a,b,c") - b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5 * (a_tile[j] - b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = 5 * ((2 * a("a,b,c")) - b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5 * ((2 * a_tile[j]) - b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = 5 * (a("a,b,c") - (3 * b("a,b,c"))));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5 * (a_tile[j] - (3 * b_tile[j])));
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") =
                             5 * ((2 * a("a,b,c")) - (3 * b("a,b,c"))));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5 * ((2 * a_tile[j]) - (3 * b_tile[j])));
    } else {
      BOOST_CHECK(a.is_zero(i) && b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(scale_sub_permute) {
  Permutation perm({2, 1, 0});

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = 5*(2 * a("c,b,a")) - (3 * b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    const size_t perm_index = c.range().ordinal(perm * a.range().idx(i));
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
              a.is_zero(perm_index) ? make_zero_tile<TSpArrayI>(c_tile.range())
                                    : perm * a.find(perm_index).get();
      TSpArrayI::value_type b_tile =
              b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                           : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5*(2 * a_tile[j]) - (3 * b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(perm_index) && b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(mult) {
  BOOST_REQUIRE_NO_THROW(c("a,b,c") = a("a,b,c") * b("a,b,c"));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], a_tile[j] * b_tile[j]);
    } else {
      BOOST_CHECK(a.is_zero(i) || b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = (2 * a("a,b,c")) * b("a,b,c"));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], (2 * a_tile[j]) * b_tile[j]);
    } else {
      BOOST_CHECK(a.is_zero(i) || b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = a("a,b,c") * (3 * b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], a_tile[j] * (3 * b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(i) || b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = (2 * a("a,b,c")) * (3 * b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], (2 * a_tile[j]) * (3 * b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(i) || b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(mult_permute) {
  Permutation perm({2, 1, 0});

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = (2 * a("c,b,a")) * (3 * b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    const size_t perm_index = c.range().ordinal(perm * a.range().idx(i));
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
              a.is_zero(perm_index) ? make_zero_tile<TSpArrayI>(c_tile.range())
                                    : perm * a.find(perm_index).get();
      TSpArrayI::value_type b_tile =
              b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                           : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], (2 * a_tile[j]) * (3 * b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(perm_index) || b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(mult_to) {
  c("a,b,c") = a("a,b,c");
  BOOST_REQUIRE_NO_THROW(c("a,b,c") *= b("a,b,c"));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], a_tile[j] * b_tile[j]);
    } else {
      BOOST_CHECK(a.is_zero(i) || b.is_zero(i));
    }
  }

  c("a,b,c") = a("a,b,c");
  BOOST_REQUIRE_NO_THROW(c("a,b,c") = c("a,b,c") * b("a,b,c"));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
              a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                           : a.find(i).get();
      TSpArrayI::value_type b_tile =
              b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                           : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], a_tile[j] * b_tile[j]);
    } else {
      BOOST_CHECK(a.is_zero(i) || b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(scale_mult) {
  BOOST_REQUIRE_NO_THROW(c("a,b,c") = 5 * (a("a,b,c") * b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5 * (a_tile[j] * b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(i) || b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = 5 * ((2 * a("a,b,c")) * b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5 * ((2 * a_tile[j]) * b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(i) || b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = 5 * (a("a,b,c") * (3 * b("a,b,c"))));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5 * (a_tile[j] * (3 * b_tile[j])));
    } else {
      BOOST_CHECK(a.is_zero(i) || b.is_zero(i));
    }
  }

  BOOST_REQUIRE_NO_THROW(c("a,b,c") =
                             5 * ((2 * a("a,b,c")) * (3 * b("a,b,c"))));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
          a.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : a.find(i).get();
      TSpArrayI::value_type b_tile =
          b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                       : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5 * ((2 * a_tile[j]) * (3 * b_tile[j])));
    } else {
      BOOST_CHECK(a.is_zero(i) || b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(scale_mult_permute) {
  Permutation perm({2, 1, 0});

  BOOST_REQUIRE_NO_THROW(c("a,b,c") = 5*(2 * a("c,b,a")) * (3 * b("a,b,c")));

  for (std::size_t i = 0ul; i < c.size(); ++i) {
    const size_t perm_index = c.range().ordinal(perm * a.range().idx(i));
    if (!c.is_zero(i)) {
      TSpArrayI::value_type c_tile = c.find(i).get();
      TSpArrayI::value_type a_tile =
              a.is_zero(perm_index) ? make_zero_tile<TSpArrayI>(c_tile.range())
                                    : perm * a.find(perm_index).get();
      TSpArrayI::value_type b_tile =
              b.is_zero(i) ? make_zero_tile<TSpArrayI>(c_tile.range())
                           : b.find(i).get();

      for (std::size_t j = 0ul; j < c_tile.size(); ++j)
        BOOST_CHECK_EQUAL(c_tile[j], 5*(2 * a_tile[j]) * (3 * b_tile[j]));
    } else {
      BOOST_CHECK(a.is_zero(perm_index) || b.is_zero(i));
    }
  }
}

BOOST_AUTO_TEST_CASE(cont) {
  const std::size_t m = a.trange().elements_range().extent(0);
  const std::size_t k = a.trange().elements_range().extent(1) *
                        a.trange().elements_range().extent(2);
  const std::size_t n = b.trange().elements_range().extent(2);

  TiledArray::EigenMatrixXi left(m, k);
  left.fill(0);

  for (TSpArrayI::const_iterator it = a.begin(); it != a.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 3> i;

    for (i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0);
         ++i[0]) {
      const std::size_t r = i[0];
      for (i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1);
           ++i[1]) {
        for (i[2] = tile.range().lobound(2); i[2] < tile.range().upbound(2);
             ++i[2]) {
          const std::size_t c = i[1] * a.trange().elements_range().stride(1) +
                                i[2] * a.trange().elements_range().stride(2);

          left(r, c) = tile[i];
        }
      }
    }
  }

  GlobalFixture::world->gop.sum(&left(0, 0), left.rows() * left.cols());

  TiledArray::EigenMatrixXi right(n, k);
  right.fill(0);

  for (TSpArrayI::const_iterator it = b.begin(); it != b.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 3> i;

    for (i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0);
         ++i[0]) {
      const std::size_t r = i[0];
      for (i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1);
           ++i[1]) {
        for (i[2] = tile.range().lobound(2); i[2] < tile.range().upbound(2);
             ++i[2]) {
          const std::size_t c = i[1] * a.trange().elements_range().stride(1) +
                                i[2] * a.trange().elements_range().stride(2);

          right(r, c) = tile[i];
        }
      }
    }
  }

  GlobalFixture::world->gop.sum(&right(0, 0), right.rows() * right.cols());

  TiledArray::EigenMatrixXi result(m, n);

  result = left * right.transpose();

  BOOST_REQUIRE_NO_THROW(w("i,j") = a("i,b,c") * b("j,b,c"));
  for (TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for (i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0);
         ++i[0]) {
      for (i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1);
           ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]));
      }
    }
  }

  BOOST_REQUIRE_NO_THROW(w("i,j") = (2 * a("i,b,c")) * b("j,b,c"));
  for (TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for (i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0);
         ++i[0]) {
      for (i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1);
           ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]) * 2);
      }
    }
  }

  BOOST_REQUIRE_NO_THROW(w("i,j") = a("i,b,c") * (3 * b("j,b,c")));

  for (TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for (i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0);
         ++i[0]) {
      for (i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1);
           ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]) * 3);
      }
    }
  }

  BOOST_REQUIRE_NO_THROW(w("i,j") = (2 * a("i,b,c")) * (3 * b("j,b,c")));

  for (TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for (i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0);
         ++i[0]) {
      for (i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1);
           ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]) * 6);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE( cont_permute )
{
  const std::size_t m = a.trange().elements_range().extent(0);
  const std::size_t k = a.trange().elements_range().extent(1) * a.trange().elements_range().extent(2);
  const std::size_t n = b.trange().elements_range().extent(2);

  TiledArray::EigenMatrixXi left(m, k);
  left.fill(0);

  for(TSpArrayI::const_iterator it = a.begin(); it != a.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 3> i;

    for(i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0); ++i[0]) {
      const std::size_t r = i[0];
      for(i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1); ++i[1]) {
        for(i[2] = tile.range().lobound(2); i[2] < tile.range().upbound(2); ++i[2]) {
          const std::size_t c = i[1] * a.trange().elements_range().stride(1)
                                + i[2] * a.trange().elements_range().stride(2);

          left(r, c) = tile[i];
        }
      }
    }
  }

  GlobalFixture::world->gop.sum(& left(0,0), left.rows() * left.cols());

  TiledArray::EigenMatrixXi right(n, k);
  right.fill(0);

  for(TSpArrayI::const_iterator it = b.begin(); it != b.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 3> i;

    for(i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0); ++i[0]) {
      const std::size_t r = i[0];
      for(i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1); ++i[1]) {
        for(i[2] = tile.range().lobound(2); i[2] < tile.range().upbound(2); ++i[2]) {
          const std::size_t c = i[2] * a.trange().elements_range().stride(1)
                                + i[1] * a.trange().elements_range().stride(2);

          right(r, c) = tile[i];
        }
      }
    }
  }

  GlobalFixture::world->gop.sum(& right(0,0), right.rows() * right.cols());

  TiledArray::EigenMatrixXi result(m, n);

  result = left * right.transpose();

  BOOST_REQUIRE_NO_THROW(w("i,j") = a("i,b,c") * b("j,c,b"));
  for(TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for(i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0); ++i[0]) {
      for(i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1); ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]));
      }
    }
  }

  BOOST_REQUIRE_NO_THROW(w("i,j") = (2 * a("i,b,c")) * b("j,c,b") );
  for(TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for(i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0); ++i[0]) {
      for(i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1); ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]) * 2);
      }
    }
  }

  BOOST_REQUIRE_NO_THROW(w("i,j") = a("i,b,c") * (3 * b("j,c,b")));

  for(TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for(i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0); ++i[0]) {
      for(i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1); ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]) * 3);
      }
    }
  }

  BOOST_REQUIRE_NO_THROW(w("i,j") = (2 * a("i,b,c")) * (3 * b("j,c,b")));

  for(TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for(i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0); ++i[0]) {
      for(i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1); ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]) * 6);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(scale_cont) {
  const std::size_t m = a.trange().elements_range().extent(0);
  const std::size_t k = a.trange().elements_range().extent(1) *
                        a.trange().elements_range().extent(2);
  const std::size_t n = b.trange().elements_range().extent(2);

  TiledArray::EigenMatrixXi left(m, k);
  left.fill(0);

  for (TSpArrayI::const_iterator it = a.begin(); it != a.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 3> i;

    for (i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0);
         ++i[0]) {
      const std::size_t r = i[0];
      for (i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1);
           ++i[1]) {
        for (i[2] = tile.range().lobound(2); i[2] < tile.range().upbound(2);
             ++i[2]) {
          const std::size_t c = i[1] * a.trange().elements_range().stride(1) +
                                i[2] * a.trange().elements_range().stride(2);

          left(r, c) = tile[i];
        }
      }
    }
  }

  GlobalFixture::world->gop.sum(&left(0, 0), left.rows() * left.cols());

  TiledArray::EigenMatrixXi right(n, k);
  right.fill(0);

  for (TSpArrayI::const_iterator it = b.begin(); it != b.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 3> i;

    for (i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0);
         ++i[0]) {
      const std::size_t r = i[0];
      for (i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1);
           ++i[1]) {
        for (i[2] = tile.range().lobound(2); i[2] < tile.range().upbound(2);
             ++i[2]) {
          const std::size_t c = i[1] * a.trange().elements_range().stride(1) +
                                i[2] * a.trange().elements_range().stride(2);

          right(r, c) = tile[i];
        }
      }
    }
  }

  GlobalFixture::world->gop.sum(&right(0, 0), right.rows() * right.cols());

  TiledArray::EigenMatrixXi result(m, n);

  result = left * right.transpose();

  BOOST_REQUIRE_NO_THROW(w("i,j") = 5 * (a("i,b,c") * b("j,b,c")));

  for (TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for (i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0);
         ++i[0]) {
      for (i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1);
           ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]) * 5);
      }
    }
  }

  BOOST_REQUIRE_NO_THROW(w("i,j") = 5 * ((2 * a("i,b,c")) * b("j,b,c")));

  for (TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for (i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0);
         ++i[0]) {
      for (i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1);
           ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]) * 10);
      }
    }
  }

  BOOST_REQUIRE_NO_THROW(w("i,j") = 5 * (a("i,b,c") * (3 * b("j,b,c"))));

  for (TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for (i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0);
         ++i[0]) {
      for (i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1);
           ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]) * 15);
      }
    }
  }

  BOOST_REQUIRE_NO_THROW(w("i,j") = 5 * ((2 * a("i,b,c")) * (3 * b("j,b,c"))));

  for (TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for (i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0);
         ++i[0]) {
      for (i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1);
           ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]) * 30);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE( scale_cont_permute )
{
  const std::size_t m = a.trange().elements_range().extent(0);
  const std::size_t k = a.trange().elements_range().extent(1) * a.trange().elements_range().extent(2);
  const std::size_t n = b.trange().elements_range().extent(2);

  TiledArray::EigenMatrixXi left(m, k);
  left.fill(0);

  for(TSpArrayI::const_iterator it = a.begin(); it != a.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 3> i;

    for(i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0); ++i[0]) {
      const std::size_t r = i[0];
      for(i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1); ++i[1]) {
        for(i[2] = tile.range().lobound(2); i[2] < tile.range().upbound(2); ++i[2]) {
          const std::size_t c = i[1] * a.trange().elements_range().stride(1)
                                + i[2] * a.trange().elements_range().stride(2);

          left(r, c) = tile[i];
        }
      }
    }
  }

  GlobalFixture::world->gop.sum(& left(0,0), left.rows() * left.cols());

  TiledArray::EigenMatrixXi right(n, k);
  right.fill(0);

  for(TSpArrayI::const_iterator it = b.begin(); it != b.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 3> i;

    for(i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0); ++i[0]) {
      const std::size_t r = i[0];
      for(i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1); ++i[1]) {
        for(i[2] = tile.range().lobound(2); i[2] < tile.range().upbound(2); ++i[2]) {
          const std::size_t c = i[2] * a.trange().elements_range().stride(1)
                                + i[1] * a.trange().elements_range().stride(2);

          right(r, c) = tile[i];
        }
      }
    }
  }

  GlobalFixture::world->gop.sum(& right(0,0), right.rows() * right.cols());

  TiledArray::EigenMatrixXi result(m, n);

  result = left * right.transpose();

  BOOST_REQUIRE_NO_THROW(w("i,j") = 5 * (a("i,b,c") * b("j,c,b")));

  for(TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for(i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0); ++i[0]) {
      for(i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1); ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]) * 5);
      }
    }
  }

  BOOST_REQUIRE_NO_THROW(w("i,j") = 5 * ((2 * a("i,b,c")) * b("j,c,b")) );

  for(TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for(i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0); ++i[0]) {
      for(i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1); ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]) * 10);
      }
    }
  }

  BOOST_REQUIRE_NO_THROW(w("i,j") = 5 * (a("i,b,c") * (3 * b("j,c,b"))));

  for(TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for(i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0); ++i[0]) {
      for(i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1); ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]) * 15);
      }
    }
  }

  BOOST_REQUIRE_NO_THROW(w("i,j") = 5 * ((2 * a("i,b,c")) * (3 * b("j,c,b"))));

  for(TSpArrayI::const_iterator it = w.begin(); it != w.end(); ++it) {
    TSpArrayI::value_type tile = *it;

    std::array<std::size_t, 2> i;

    for(i[0] = tile.range().lobound(0); i[0] < tile.range().upbound(0); ++i[0]) {
      for(i[1] = tile.range().lobound(1); i[1] < tile.range().upbound(1); ++i[1]) {
        BOOST_CHECK_EQUAL(tile[i], result(i[0], i[1]) * 30);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(cont_non_uniform1) {
  // Construct the tiled range
  std::array<std::size_t, 6> tiling1 = {{0, 1, 2, 3, 4, 5}};
  std::array<std::size_t, 2> tiling2 = {{0, 40}};
  TiledRange1 tr1_1(tiling1.begin(), tiling1.end());
  TiledRange1 tr1_2(tiling2.begin(), tiling2.end());
  std::array<TiledRange1, 4> tiling4 = {{tr1_1, tr1_2, tr1_1, tr1_1}};
  TiledRange trange(tiling4.begin(), tiling4.end());

  const std::size_t m = 5;
  const std::size_t k = 40 * 5 * 5;
  const std::size_t n = 5;

  // Construct the test arguments
  TSpArrayI left(*GlobalFixture::world, trange,
                 make_random_sparseshape(trange));
  TSpArrayI right(*GlobalFixture::world, trange,
                  make_random_sparseshape(trange));

  // Construct the reference matrices
  TiledArray::EigenMatrixXi left_ref(m, k);
  TiledArray::EigenMatrixXi right_ref(n, k);

  // Initialize input
  rand_fill_matrix_and_array(left_ref, left, 23);
  rand_fill_matrix_and_array(right_ref, right, 42);

  // Compute the reference result
  TiledArray::EigenMatrixXi result_ref = 5 * left_ref * right_ref.transpose();

  // Compute the result to be tested
  TSpArrayI result;
  BOOST_REQUIRE_NO_THROW(result("x,y") =
                             5 * left("x,i,j,k") * right("y,i,j,k"));

  // Check the result
  for (TSpArrayI::iterator it = result.begin(); it != result.end(); ++it) {
    const TSpArrayI::value_type tile = *it;
    for (Range::const_iterator rit = tile.range().begin();
         rit != tile.range().end(); ++rit) {
      const std::size_t elem_index = result.elements_range().ordinal(*rit);
      BOOST_CHECK_EQUAL(result_ref.array()(elem_index), tile[*rit]);
    }
  }
}

BOOST_AUTO_TEST_CASE(cont_non_uniform2) {
  // Construct the tiled range
  std::array<std::size_t, 6> tiling1 = {{0, 1, 2, 3, 4, 5}};
  std::array<std::size_t, 2> tiling2 = {{0, 40}};
  TiledRange1 tr1_1(tiling1.begin(), tiling1.end());
  TiledRange1 tr1_2(tiling2.begin(), tiling2.end());
  std::array<TiledRange1, 4> tiling4 = {{tr1_1, tr1_1, tr1_2, tr1_2}};
  TiledRange trange(tiling4.begin(), tiling4.end());

  const std::size_t m = 5;
  const std::size_t k = 5 * 40 * 40;
  const std::size_t n = 5;

  // Construct the test arguments
  TSpArrayI left(*GlobalFixture::world, trange,
                 make_random_sparseshape(trange));
  TSpArrayI right(*GlobalFixture::world, trange,
                  make_random_sparseshape(trange));

  // Construct the reference matrices
  TiledArray::EigenMatrixXi left_ref(m, k);
  TiledArray::EigenMatrixXi right_ref(n, k);

  // Initialize input
  rand_fill_matrix_and_array(left_ref, left, 23);
  rand_fill_matrix_and_array(right_ref, right, 42);

  // Compute the reference result
  TiledArray::EigenMatrixXi result_ref = 5 * left_ref * right_ref.transpose();

  // Compute the result to be tested
  TSpArrayI result;
  BOOST_REQUIRE_NO_THROW(result("x,y") =
                             5 * left("x,i,j,k") * right("y,i,j,k"));

  // Check the result
  for (TSpArrayI::iterator it = result.begin(); it != result.end(); ++it) {
    const TSpArrayI::value_type tile = *it;
    for (Range::const_iterator rit = tile.range().begin();
         rit != tile.range().end(); ++rit) {
      const std::size_t elem_index = result.elements_range().ordinal(*rit);
      BOOST_CHECK_EQUAL(result_ref.array()(elem_index), tile[*rit]);
    }
  }
}

BOOST_AUTO_TEST_CASE(cont_plus_reduce) {
  // Construct the tiled range
  std::array<std::size_t, 6> tiling1 = {{0, 1, 2, 3, 4, 5}};
  std::array<std::size_t, 2> tiling2 = {{0, 40}};
  TiledRange1 tr1_1(tiling1.begin(), tiling1.end());
  TiledRange1 tr1_2(tiling2.begin(), tiling2.end());
  std::array<TiledRange1, 4> tiling4 = {{tr1_1, tr1_2, tr1_1, tr1_1}};
  TiledRange trange(tiling4.begin(), tiling4.end());

  const std::size_t m = 5;
  const std::size_t k = 40 * 5 * 5;
  const std::size_t n = 5;

  // Construct the test arrays
  TSpArrayI arg1(*GlobalFixture::world, trange,
                 make_random_sparseshape(trange));
  TSpArrayI arg2(*GlobalFixture::world, trange,
                 make_random_sparseshape(trange));
  TSpArrayI arg3(*GlobalFixture::world, trange,
                 make_random_sparseshape(trange));
  TSpArrayI arg4(*GlobalFixture::world, trange,
                 make_random_sparseshape(trange));

  // Construct the reference matrices
  TiledArray::EigenMatrixXi arg1_ref(m, k);
  TiledArray::EigenMatrixXi arg2_ref(n, k);
  TiledArray::EigenMatrixXi arg3_ref(m, k);
  TiledArray::EigenMatrixXi arg4_ref(n, k);

  // Initialize input
  rand_fill_matrix_and_array(arg1_ref, arg1, 23);
  rand_fill_matrix_and_array(arg2_ref, arg2, 42);
  rand_fill_matrix_and_array(arg3_ref, arg3, 79);
  rand_fill_matrix_and_array(arg4_ref, arg4, 19);

  // Compute the reference result
  TiledArray::EigenMatrixXi result_ref =
      2 * (arg1_ref * arg2_ref.transpose() + arg1_ref * arg4_ref.transpose() +
           arg3_ref * arg4_ref.transpose() + arg3_ref * arg2_ref.transpose());

  // Compute the result to be tested
  TSpArrayI result;
  result("x,y") = arg1("x,i,j,k") * arg2("y,i,j,k");
  result("x,y") += arg3("x,i,j,k") * arg4("y,i,j,k");
  result("x,y") += arg1("x,i,j,k") * arg4("y,i,j,k");
  result("x,y") += arg3("x,i,j,k") * arg2("y,i,j,k");
  result("x,y") += arg3("x,i,j,k") * arg2("y,i,j,k");
  result("x,y") += arg1("x,i,j,k") * arg2("y,i,j,k");
  result("x,y") += arg3("x,i,j,k") * arg4("y,i,j,k");
  result("x,y") += arg1("x,i,j,k") * arg4("y,i,j,k");

  // Check the result
  for (TSpArrayI::iterator it = result.begin(); it != result.end(); ++it) {
    const TSpArrayI::value_type tile = *it;
    for (Range::const_iterator rit = tile.range().begin();
         rit != tile.range().end(); ++rit) {
      const std::size_t elem_index = result.elements_range().ordinal(*rit);
      BOOST_CHECK_EQUAL(result_ref.array()(elem_index), tile[*rit]);
    }
  }
}

BOOST_AUTO_TEST_CASE(no_alias_plus_reduce) {
  // Construct the tiled range
  std::array<std::size_t, 6> tiling1 = {{0, 1, 2, 3, 4, 5}};
  std::array<std::size_t, 2> tiling2 = {{0, 40}};
  TiledRange1 tr1_1(tiling1.begin(), tiling1.end());
  TiledRange1 tr1_2(tiling2.begin(), tiling2.end());
  std::array<TiledRange1, 4> tiling4 = {{tr1_1, tr1_2, tr1_1, tr1_1}};
  TiledRange trange(tiling4.begin(), tiling4.end());

  const std::size_t m = 5;
  const std::size_t k = 40 * 5 * 5;
  const std::size_t n = 5;

  // Construct the test arrays
  TSpArrayI arg1(*GlobalFixture::world, trange,
                 make_random_sparseshape(trange));
  TSpArrayI arg2(*GlobalFixture::world, trange,
                 make_random_sparseshape(trange));
  TSpArrayI arg3(*GlobalFixture::world, trange,
                 make_random_sparseshape(trange));
  TSpArrayI arg4(*GlobalFixture::world, trange,
                 make_random_sparseshape(trange));

  // Construct the reference matrices
  TiledArray::EigenMatrixXi arg1_ref(m, k);
  TiledArray::EigenMatrixXi arg2_ref(n, k);
  TiledArray::EigenMatrixXi arg3_ref(m, k);
  TiledArray::EigenMatrixXi arg4_ref(n, k);

  // Initialize input
  rand_fill_matrix_and_array(arg1_ref, arg1, 23);
  rand_fill_matrix_and_array(arg2_ref, arg2, 42);
  rand_fill_matrix_and_array(arg3_ref, arg3, 79);
  rand_fill_matrix_and_array(arg4_ref, arg4, 19);

  // Compute the reference result
  TiledArray::EigenMatrixXi result_ref =
      2 * (arg1_ref * arg2_ref.transpose() + arg1_ref * arg4_ref.transpose() +
           arg3_ref * arg4_ref.transpose() + arg3_ref * arg2_ref.transpose());

  // Compute the result to be tested
  TSpArrayI result;
  result("x,y") = arg1("x,i,j,k") * arg2("y,i,j,k");
  result("x,y").no_alias() += arg3("x,i,j,k") * arg4("y,i,j,k");
  result("x,y").no_alias() += arg1("x,i,j,k") * arg4("y,i,j,k");
  result("x,y").no_alias() += arg3("x,i,j,k") * arg2("y,i,j,k");
  result("x,y").no_alias() += arg3("x,i,j,k") * arg2("y,i,j,k");
  result("x,y").no_alias() += arg1("x,i,j,k") * arg2("y,i,j,k");
  result("x,y").no_alias() += arg3("x,i,j,k") * arg4("y,i,j,k");
  result("x,y").no_alias() += arg1("x,i,j,k") * arg4("y,i,j,k");

  // Check the result
  for (TSpArrayI::iterator it = result.begin(); it != result.end(); ++it) {
    const TSpArrayI::value_type tile = *it;
    for (Range::const_iterator rit = tile.range().begin();
         rit != tile.range().end(); ++rit) {
      const std::size_t elem_index = result.elements_range().ordinal(*rit);
      BOOST_CHECK_EQUAL(result_ref.array()(elem_index), tile[*rit]);
    }
  }
}

BOOST_AUTO_TEST_CASE(outer_product) {
  // Generate Eigen matrices from input arrays.
  EigenMatrixXi ev = make_matrix(v);
  EigenMatrixXi eu = make_matrix(u);

  // Generate the expected result
  EigenMatrixXi ew_test = eu * ev.transpose();

  // Test that outer product works
  BOOST_REQUIRE_NO_THROW(w("i,j") = u("i") * v("j"));

  GlobalFixture::world->gop.fence();

  EigenMatrixXi ew = make_matrix(w);

  BOOST_CHECK_EQUAL(ew, ew_test);
}

BOOST_AUTO_TEST_CASE(dot) {
  // Test the dot expression function
  int result = 0;
  BOOST_REQUIRE_NO_THROW(result = a("a,b,c") * b("a,b,c"));
  BOOST_REQUIRE_NO_THROW(result += a("a,b,c") * b("a,b,c"));
  BOOST_REQUIRE_NO_THROW(result -= a("a,b,c") * b("a,b,c"));
  BOOST_REQUIRE_NO_THROW(result *= a("a,b,c") * b("a,b,c"));
  BOOST_REQUIRE_NO_THROW(result = a("a,b,c").dot(b("a,b,c")).get());

  // Compute the expected value for the dot function.
  int expected = 0;
  for (std::size_t i = 0ul; i < a.size(); ++i) {
    if (!a.is_zero(i) && !b.is_zero(i)) {
      TSpArrayI::value_type a_tile = a.find(i).get();
      TSpArrayI::value_type b_tile = b.find(i).get();

      for (std::size_t j = 0ul; j < a_tile.size(); ++j)
        expected += a_tile[j] * b_tile[j];
    }
  }

  // Check the result of dot
  BOOST_CHECK_EQUAL(result, expected);
}

BOOST_AUTO_TEST_CASE(dot_permute) {
  Permutation perm({2, 1, 0});
  // Test the dot expression function
  int result = 0;
  BOOST_REQUIRE_NO_THROW(result = a("a,b,c") * b("c,b,a"));
  BOOST_REQUIRE_NO_THROW(result += a("a,b,c") * b("c,b,a"));
  BOOST_REQUIRE_NO_THROW(result -= a("a,b,c") * b("c,b,a"));
  BOOST_REQUIRE_NO_THROW(result *= a("a,b,c") * b("c,b,a"));
  BOOST_REQUIRE_NO_THROW(result = a("a,b,c").dot(b("c,b,a")).get());

  // Compute the expected value for the dot function.
  int expected = 0;
  for (std::size_t i = 0ul; i < a.size(); ++i) {
    const size_t perm_index = a.range().ordinal(perm * b.range().idx(i));
    if (!a.is_zero(i) && !b.is_zero(perm_index)) {
      TSpArrayI::value_type a_tile = a.find(i).get();
      TSpArrayI::value_type b_tile = perm * b.find(perm_index).get();

      for (std::size_t j = 0ul; j < a_tile.size(); ++j)
        expected += a_tile[j] * b_tile[j];
    }
  }

  // Check the result of dot
  BOOST_CHECK_EQUAL(result, expected);
}

BOOST_AUTO_TEST_CASE(dot_expr) {
  // Test the dot expression function
  int result = 0;
  BOOST_REQUIRE_NO_THROW(result = a("a,b,c") * b("a,b,c"));

  // Compute the expected value for the dot function.
  int expected = 0;
  for (std::size_t i = 0ul; i < a.size(); ++i) {
    if (!a.is_zero(i) && !b.is_zero(i)) {
      TSpArrayI::value_type a_tile = a.find(i).get();
      TSpArrayI::value_type b_tile = b.find(i).get();

      for (std::size_t j = 0ul; j < a_tile.size(); ++j)
        expected += a_tile[j] * b_tile[j];
    }
  }

  // Check the result of dot
  BOOST_CHECK_EQUAL(result, expected);
}

BOOST_AUTO_TEST_CASE(dot_contr) {
  for (int i = 0; i != 50; ++i)
    BOOST_REQUIRE_NO_THROW(
        (a("a,b,c") * b("d,b,c")).dot(b("d,e,f") * a("a,e,f")));
}

BOOST_AUTO_TEST_SUITE_END()
