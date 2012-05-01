#ifndef TILEDARRAY_CYCLIC_PMAP_H__INCLUDED
#define TILEDARRAY_CYCLIC_PMAP_H__INCLUDED

#include <TiledArray/pmap.h>
#include <cmath>
#include <world/world.h>

namespace TiledArray {
  namespace detail {

    /// Map processes using cyclic decomposition

    /// This map does a cyclic decomposition of an m-by-n matrix. It distributes
    /// processors into an x-by-y matrix such that the ratios of m/n and x/y are
    /// approximately equal. It also applies a sudo-randomization to the
    /// resulting process map.
    class CyclicPmap : public Pmap<std::size_t> {
    public:
      typedef Pmap<std::size_t>::key_type key_type;
      typedef Pmap<std::size_t>::const_iterator const_iterator;

      CyclicPmap(madness::World& world, std::size_t m, std::size_t n) :
          rank_(world.rank()),
          procs_(world.size()),
          m_(m),
          n_(n),
          x_(),
          y_(),
          seed_(0ul),
          local_()
      {
        TA_ASSERT(m > 0ul);
        TA_ASSERT(n > 0ul);

        // Get a rough estimate of the process dimensions
        // The ratios of x_ / y_  and m_ / n_ should be approximately equal.
        x_ = std::sqrt(procs_ * m / n);
        y_ = procs_ / x_;

        // Get the number of process not included.
        std::size_t p = procs_ - (x_ * y_);

        // Try and reclaim the remaining processes
        if(p > x_) {
          y_ += p / x_;
        } else if(p > y_) {
          x_ += p / y_;
        }

        // The process map should be no bigger than m x n
        x_ = std::min<std::size_t>(x_, m_);
        y_ = std::min<std::size_t>(y_, n_);
      }


      virtual ~CyclicPmap() { }

      /// Set the hashing seed
      virtual void set_seed(madness::hashT seed = 0ul) {
        seed_ = seed;

        // Construct the local process map.
        // Todo: This iterates over all elements of the map, but it could be more
        // efficient.
        local_.resize((m_ / x_) * (n_ / y_), 0);
        const key_type size = m_ * n_;
        for(key_type i = 0ul; i < size; ++i) {
          if(this->owner(i) == rank_)
            local_.push_back(i);
        }
      }

      /// Key owner

      /// \param key The element key to be mapped
      /// \return The \c ProcessID of the process that owns \c key .
      virtual ProcessID owner(const key_type& key) const {
        TA_ASSERT(key < (m_ * n_));
        // Get the position
        const std::size_t o = ((key / n_) % x_) * y_ + ((key % n_) % y_);

        // sudo-randomize the owning process
        madness::hashT seed = seed_;
        madness::hash_combine(seed, o);
        return seed % procs_;

      }


      /// Local size accessor

      /// \return The number of local elements
      virtual std::size_t local_size() const { return local_.size(); }

      /// Local elements

      /// \return \c true when there are no local elements, otherwise \c false .
      virtual bool empty() const { return local_.empty(); }

      /// Begin local element iterator

      /// \return An iterator that points to the beginning of the local element set
      virtual const_iterator begin() const { return local_.begin(); }


      /// End local element iterator

      /// \return An iterator that points to the beginning of the local element set
      virtual const_iterator end() const { return local_.end(); }

    private:
      std::size_t procs_; ///< Number of processes in the world
      std::size_t rank_; ///< This process's rank
      std::size_t m_; ///< Number of rows to be mapped
      std::size_t n_; ///< Number of columns to be mapped
      std::size_t x_; ///< Number of process rows
      std::size_t y_; ///< Number of process columns
      madness::hashT seed_; ///< Hashing seed for process randomization
      std::vector<key_type> local_; ///< A vector of local elements in map.
    }; // class CyclicPmap

  }  // namespace detail
}  // namespace TiledArray


#endif // TILEDARRAY_CYCLIC_PMAP_H__INCLUDED
