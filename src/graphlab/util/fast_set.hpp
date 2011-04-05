#ifndef GRAPHLAB_FAST_SET_HPP
#define GRAPHLAB_FAST_SET_HPP


#include <iostream>
#include <set>
#include <iterator>

#include <graphlab/serialization/iarchive.hpp>
#include <graphlab/serialization/oarchive.hpp>



#include <graphlab/macros_def.hpp>
namespace graphlab {


  /**
   * This class implements a dense set of fixed maximum size which
   * support quick operations with stack allocation.
   */
  template<size_t MAX_DIM, typename T>
  class fast_set {
  public: // typedefs
    typedef T value_type;
    typedef T* iterator;
    typedef const T* const_iterator;
    enum sizes {max_dim_type = MAX_DIM };


    template< size_t T1, size_t T2 >
    struct max_type { enum max_value { value = T1 < T2? T2 : T1 }; };


  public:
    //! Construct an empty set
    fast_set() : nelems(0) { }

    //! Create a stack set with just one element
    fast_set(const T& elem) : nelems(1) { values[0] = elem; }
    
    /**
     * Create a stack from an std set by adding each element one at a
     * time
     */
    template<typename OtherT>
    fast_set(const std::set<OtherT>& other) : nelems(other.size()) { 
      ASSERT_LE(nelems, MAX_DIM);
      size_t index = 0;
      foreach(const OtherT& elem, other) values[index++] = elem;
    }


    /**
     * Create a stack from an std set by adding each element one at a
     * time
     */
    template<size_t OtherSize>
    fast_set(const fast_set<OtherSize, T>& other) : nelems(other.size()) { 
      ASSERT_LE(nelems, MAX_DIM);
      size_t index = 0;
      for(const T* elem = other.begin(); elem != other.end(); ++elem) 
        values[index++] = *elem;
    }


    //! Get the begin iterator
    inline T* begin() { return values; }

    //! get the end iterator
    inline T* end() { return values + nelems; }


    //! Get the begin iterator
    const T* begin() const { return values; }

    //! Get the end iterator
    const T* end() const { return values + nelems; }

    //! get the size of the set
    inline size_t size() const { return nelems; }

    //! determine if there are any elements in the set
    bool empty() const { return size() == 0; }

    
    //! test whether the set contains the given element
    bool contains(const T& elem) const {
      return std::binary_search(begin(), end(), elem);
    }

    //! test whether the set contains the given set of element
    template<size_t OtherDim>
    bool contains(const fast_set<OtherDim, T>& other) const {
      return std::includes(begin(), end(), 
                           other.begin(), other.end());
    }


    /**
     * Test if this set is contained in other.  If so this returns
     * true. 
     */
    template<size_t OtherDim>
    bool operator<=(const fast_set<OtherDim, T>& other) const {
      return contains(other);
    }

    template<size_t OtherDim>
    bool operator==(const fast_set<OtherDim, T>& other) const {
      if(size() != other.size()) return false;
      return std::equal(begin(), end(), other.begin());
    }



    //! insert an element into this set
    void insert(const T& elem) {
      *this += elem;
    }


    //! insert a range of elements into this set
    void insert(const T* begin, const T* end) {
      // Ensure that other size is not negative
      ASSERT_LE(begin, end);
      // Ensure that the other set has an appropriate size
      const size_t other_size = end - begin;  
      ASSERT_LE(other_size, MAX_DIM);
      // Construct a temporary fast set representing the range
      fast_set other;
      for(size_t i = 0; i < other_size; ++i) {
        other[i] = begin[i];
        // Ensure that the other set is sorted
        if(i+1 < other_size) ASSERT_LT(begin[i], begin[i+1]);
      }
      // Insert it into this fast set using the + operation
      *this += other;
    }


    //! remove an element from the set
    void erase(const T& elem) { *this -= elem; }
    

    //! get the element at a particular location
    const T& operator[](size_t index) const {
      ASSERT_LT(index, nelems);
      return values[index];
    }


    

    // //! Take the union of two sets
    // inline fast_set operator+(const fast_set& other) const {
    //   fast_set result;
    //   size_t i = 0, j = 0;
    //   while(i < size() && j < other.size()) {
    //     assert(result.nelems < MAX_DIM);
    //     if(values[i] < other.values[j])  // This comes first
    //       result.values[result.nelems++] = values[i++];
    //     else if (values[i] > other.values[j])  // other comes first
    //       result.values[result.nelems++] = other.values[j++];
    //     else {  // both are same
    //       result.values[result.nelems++] = values[i++]; j++;
    //     }
    //   }
    //   // finish writing this
    //   while(i < size()) {
    //     assert(result.nelems < MAX_DIM);
    //     result.values[result.nelems++] = values[i++];
    //   }
    //   // finish writing other
    //   while(j < other.size()) {
    //     assert(result.nelems < MAX_DIM);
    //     result.values[result.nelems++] = other.values[j++];
    //   }
    //   return result;
    // }

    //! Take the union of two sets
    template<size_t OtherDim>
    inline fast_set< max_type<OtherDim, MAX_DIM>::value, T > 
    operator+(const fast_set<OtherDim, T>& other) const {
      typedef fast_set< max_type<OtherDim, MAX_DIM>::value, T>
        result_type;
      result_type result;
      const T* new_end = 
        std::set_union(begin(), end(),
                       other.begin(), other.end(),
                       safe_iterator(result.begin(), 
                                     result.absolute_end())).begin;
      result.nelems = new_end - result.begin();
      ASSERT_LE(result.nelems, result_type::max_dim_type);
      return result;
    }


    //! Add the other set to this set
    template<size_t OtherDim>
    inline fast_set& operator+=(const fast_set<OtherDim, T>& other) {
      *this = *this + other;
      return *this;
    }


    //! Add an element to this set. This is "optimized" since it is
    //! used frequently
    inline fast_set& operator+=(const T& elem) {
      // // Find where elem should be inserted
      // size_t index = 0;
      // for(; index < nelems && values[index] < elem; ++index);      
      T* ptr(std::lower_bound(begin(), end(), elem));     
      // if the element already exists return
      if(ptr != end() && !(elem < *ptr) ) return *this;
      // otherwise the element does not exist so add it at the current
      // location and increment the number of elements
      T tmp(elem); nelems++; // advances end
      ASSERT_LE(nelems, MAX_DIM);
      // Insert the element at index swapping out the rest of the
      // array
      for(; ptr < end(); ++ptr) std::swap(*ptr, tmp);      
      // Finished return
      return *this;
    }



    //! Remove the other set from this set
    fast_set& operator-=(const fast_set& other) {
      if(other.size() == 0) return *this;    
      // Backup the old nelems and reset nelems
      size_t old_nelems = size(); nelems = 0;
      for(size_t i = 0, j = 0; i < old_nelems; ++i ) {
        // advance the other index
        for( ; j < other.size() && values[i] > other.values[j]; ++j);
        // otherwise check equality or move forward
        if(j >= other.size() || (values[i] != other.values[j])) 
          values[nelems++] = values[i];
      }
      assert(nelems <= MAX_DIM);
      return *this;
    }

    //! subtract the right set form the left set
    fast_set operator-(const fast_set& other) const {
      fast_set result = *this;
      result -= other;
      return result;
    }

    //! Take the intersection of two sets
    fast_set operator*(const fast_set& other) const {
      fast_set result; 
      for(size_t i = 0, j = 0;
          i < size() && j < other.size(); ) {
        if(values[i] == other.values[j]) {
          result.values[result.nelems++] = values[i++]; j++;
        } else if(values[i] < other.values[j]) i++; else j++;    
      }
      assert(result.nelems <= MAX_DIM);
      return result;
    }

    //! Take the intersection of two sets
    fast_set operator*=(const fast_set& other)  {
      *this = *this * other;
      return *this;
    }

    //! Load the set form file
    void load(iarchive& arc) {
      arc >> nelems;
      assert(nelems <= MAX_DIM);
      for(size_t i = 0; i < nelems; ++i) {
        arc >> values[i];
        if( i > 0 ) assert(values[i] > values[i-1]);
      }
    }
    
    //! Save the set to file
    void save(oarchive& arc) const {
      arc << nelems;
      for(size_t i = 0; i < nelems; ++i) arc << values[i];
    }
  private:
    size_t nelems;
    T values[MAX_DIM];

                     
    //! get the end iterator to the complete range
    inline T* absolute_end() { return values + MAX_DIM; }


    struct safe_iterator : 
      public std::iterator<std::input_iterator_tag, T>  {
      T* begin;
      const T* end;
      safe_iterator(const safe_iterator& other) :
        begin(other.begin), end(other.end) { }
      safe_iterator(T* begin, const T* end) : 
        begin(begin), end(end) {
        ASSERT_NE(begin, NULL);
        ASSERT_NE(end, NULL);
        ASSERT_LE(begin, end);
      }
      inline safe_iterator& operator++() { ++begin; return *this; }
      inline safe_iterator& operator++(int) {
        safe_iterator tmp(*this); operator++(); return tmp;
      }
      inline bool operator==(const safe_iterator& other) {
        ASSERT_EQ(end, other.end);
        return begin == other.begin;
      }
      inline bool operator!=(const safe_iterator& other) {
        return !operator==(other);
      }
      int& operator*() { ASSERT_LT(begin, end); return *begin; }
    };


    

  }; // end of fast_set
}; // end of graphlab namespace

template<size_t MAX_DIM, typename T>
std::ostream&
operator<<(std::ostream& out, const graphlab::fast_set<MAX_DIM, T>& set) {
  out << "{";
  for(size_t i = 0; i < set.size(); ++i) {
    out << set[i];
    if(i + 1 < set.size()) out << ", "; 
  }
  out << "}";
  return out;
}



#include <graphlab/macros_undef.hpp>
#endif
