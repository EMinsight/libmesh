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



// Local Includes
#include "libmesh/metis_partitioner.h"

#include "libmesh/libmesh_config.h"
#include "libmesh/elem.h"
#include "libmesh/enum_partitioner_type.h"
#include "libmesh/error_vector.h"
#include "libmesh/libmesh_logging.h"
#include "libmesh/mesh_base.h"
#include "libmesh/mesh_communication.h"
#include "libmesh/mesh_tools.h"
#include "libmesh/metis_csr_graph.h"
#include "libmesh/parallel.h"
#include "libmesh/utility.h"
#include "libmesh/vectormap.h"

#ifdef LIBMESH_HAVE_METIS
// MIPSPro 7.4.2 gets confused about these nested namespaces
# ifdef __sgi
#  include <cstdarg>
# endif
namespace Metis {
extern "C" {
#     include "libmesh/ignore_warnings.h"
#     include "metis.h"
#     include "libmesh/restore_warnings.h"
}
}
#else
#  include "libmesh/sfc_partitioner.h"
#endif


// C++ includes
#include <map>
#include <unordered_map>
#include <vector>


namespace libMesh
{


PartitionerType MetisPartitioner::type() const
{
  return METIS_PARTITIONER;
}


void MetisPartitioner::partition_range(MeshBase & mesh,
                                       MeshBase::element_iterator beg,
                                       MeshBase::element_iterator end,
                                       unsigned int n_pieces)
{
  // Check for easy returns
  if (beg == end)
    return;

  if (n_pieces == 1)
    {
      this->single_partition_range (beg, end);
      return;
    }

  libmesh_assert_greater (n_pieces, 0);

  // We don't yet support distributed meshes with this Partitioner
  if (!mesh.is_serial())
    {
      libMesh::out << "WARNING: Forced to gather a distributed mesh for METIS" << std::endl;
      mesh.allgather();
    }

  // What to do if the Metis library IS NOT present
#ifndef LIBMESH_HAVE_METIS

  libmesh_do_once(
  libMesh::out << "ERROR: The library has been built without"    << std::endl
               << "Metis support.  Using a space-filling curve"  << std::endl
               << "partitioner instead!"                         << std::endl;);

  SFCPartitioner sfcp;
  sfcp.partition_range (mesh, beg, end, n_pieces);

  // What to do if the Metis library IS present
#else

  LOG_SCOPE("partition_range()", "MetisPartitioner");

  const dof_id_type n_range_elem = cast_int<dof_id_type>(std::distance(beg, end));

  // Metis will only consider the elements in the range.
  // We need to map the range element ids into a
  // contiguous range.  Further, we want the unique range indexing to be
  // independent of the element ordering, otherwise a circular dependency
  // can result in which the partitioning depends on the ordering which
  // depends on the partitioning...
  vectormap<dof_id_type, dof_id_type> global_index_map;
  global_index_map.reserve (n_range_elem);

  {
    std::vector<dof_id_type> global_index;

    MeshCommunication().find_global_indices (mesh.comm(),
                                             MeshTools::create_bounding_box(mesh),
                                             beg, end, global_index);

    libmesh_assert_equal_to (global_index.size(), n_range_elem);

    // If we have unique_id() then global_index entries should be
    // unique, so we just need a map for them.
#ifdef LIBMESH_ENABLE_UNIQUE_ID
    std::size_t cnt=0;
    for (const auto & elem : as_range(beg, end))
      global_index_map.emplace(elem->id(), global_index[cnt++]);
#else
    // If we don't have unique_id() then Hilbert SFC based
    // global_index entries might overlap if elements overlap, so we
    // need to fix any duplicates ...
    //
    // First, check for duplicates in O(N) time
    std::unordered_map<dof_id_type, std::vector<dof_id_type>>
      global_index_inverses;
    bool found_duplicate_indices = false;
      {
        std::size_t cnt=0;
        for (const auto & elem : as_range(beg, end))
          {
            auto & vec = global_index_inverses[global_index[cnt++]];
            if (!vec.empty())
              found_duplicate_indices = true;
            vec.push_back(elem->id());
          }
      }

    // But if we find them, we'll need O(N log N) to fix them while
    // retaining the same space-filling-curve for efficient initial
    // partitioning.
    if (found_duplicate_indices)
      {
        const std::map<dof_id_type, std::vector<dof_id_type>>
          sorted_inverses(global_index_inverses.begin(),
                          global_index_inverses.end());
        std::size_t new_global_index=0;
        for (const auto & [index, elem_ids] : sorted_inverses)
          for (const dof_id_type elem_id : elem_ids)
            global_index_map.emplace(elem_id, new_global_index++);
      }
    else
      {
        std::size_t cnt=0;
        for (const auto & elem : as_range(beg, end))
          global_index_map.emplace(elem->id(), global_index[cnt++]);
      }
#endif

    libmesh_assert_equal_to (global_index_map.size(), n_range_elem);

#ifdef DEBUG
    std::unordered_set<dof_id_type> distinct_global_indices, distinct_ids;
    for (const auto & [elem_id, index] : global_index_map)
      {
        distinct_ids.insert(elem_id);
        distinct_global_indices.insert(index);
      }
    libmesh_assert_equal_to (distinct_global_indices.size(), n_range_elem);
    libmesh_assert_equal_to (distinct_ids.size(), n_range_elem);
#endif
  }

  // If we have boundary elements in this mesh, we want to account for
  // the connectivity between them and interior elements.  We can find
  // interior elements from boundary elements, but we need to build up
  // a lookup map to do the reverse.
  typedef std::unordered_multimap<const Elem *, const Elem *> map_type;
  map_type interior_to_boundary_map;

  for (const auto & elem : as_range(beg, end))
    {
      // If we don't have an interior_parent then there's nothing
      // to look us up.
      if ((elem->dim() >= LIBMESH_DIM) ||
          !elem->interior_parent())
        continue;

      // get all relevant interior elements
      std::set<Elem *> neighbor_set;
      elem->find_interior_neighbors(neighbor_set);

      for (auto & neighbor : neighbor_set)
        interior_to_boundary_map.emplace(neighbor, elem);
    }

  // Data structure that Metis will fill up on processor 0 and broadcast.
  std::vector<Metis::idx_t> part(n_range_elem);

  // Invoke METIS, but only on processor 0.
  // Then broadcast the resulting decomposition
  if (mesh.processor_id() == 0)
    {
      // Data structures and parameters needed only on processor 0 by Metis.
      // std::vector<Metis::idx_t> options(5);
      std::vector<Metis::idx_t> vwgt(n_range_elem);

      Metis::idx_t
        n = static_cast<Metis::idx_t>(n_range_elem),   // number of "nodes" (elements) in the graph
        // wgtflag = 2,                                // weights on vertices only, none on edges
        // numflag = 0,                                // C-style 0-based numbering
        nparts  = static_cast<Metis::idx_t>(n_pieces), // number of subdomains to create
        edgecut = 0;                                   // the numbers of edges cut by the resulting partition

      // Set the options
      // options[0] = 0; // use default options

      // build the graph
      METIS_CSR_Graph<Metis::idx_t> csr_graph;

      csr_graph.offsets.resize(n_range_elem + 1, 0);

      // Local scope for these
      {
        // build the graph in CSR format.  Note that
        // the edges in the graph will correspond to
        // face neighbors

#ifdef LIBMESH_ENABLE_AMR
        std::vector<const Elem *> neighbors_offspring;
#endif

#ifndef NDEBUG
        std::size_t graph_size=0;
#endif

        // (1) first pass - get the row sizes for each element by counting the number
        // of face neighbors.  Also populate the vwght array if necessary
        for (const auto & elem : as_range(beg, end))
          {
            const dof_id_type elem_global_index =
              global_index_map[elem->id()];

            libmesh_assert_less (elem_global_index, vwgt.size());

            // The weight is used to define what a balanced graph is
            if (!_weights)
              {
                // Spline nodes are a special case (storing all the
                // unconstrained DoFs in an IGA simulation), but in
                // general we'll try to distribute work by expecting
                // it to be roughly proportional to DoFs, which are
                // roughly proportional to nodes.
                if (elem->type() == NODEELEM &&
                    elem->mapping_type() == RATIONAL_BERNSTEIN_MAP)
                  vwgt[elem_global_index] = 50;
                else
                  vwgt[elem_global_index] = elem->n_nodes();
              }
            else
              vwgt[elem_global_index] = static_cast<Metis::idx_t>((*_weights)[elem->id()]);

            unsigned int num_neighbors = 0;

            // Loop over the element's neighbors.  An element
            // adjacency corresponds to a face neighbor
            for (auto neighbor : elem->neighbor_ptr_range())
              {
                if (neighbor != nullptr)
                  {
                    // If the neighbor is active, but is not in the
                    // range of elements being partitioned, treat it
                    // as a nullptr neighbor.
                    if (neighbor->active() && !global_index_map.count(neighbor->id()))
                      continue;

                    // If the neighbor is active treat it
                    // as a connection
                    if (neighbor->active())
                      num_neighbors++;

#ifdef LIBMESH_ENABLE_AMR

                    // Otherwise we need to find all of the
                    // neighbor's children that are connected to
                    // us and add them
                    else
                      {
                        // The side of the neighbor to which
                        // we are connected
                        const unsigned int ns =
                          neighbor->which_neighbor_am_i (elem);
                        libmesh_assert_less (ns, neighbor->n_neighbors());

                        // Get all the active children (& grandchildren, etc...)
                        // of the neighbor.

                        // FIXME - this is the wrong thing, since we
                        // should be getting the active family tree on
                        // our side only.  But adding too many graph
                        // links may cause hanging nodes to tend to be
                        // on partition interiors, which would reduce
                        // communication overhead for constraint
                        // equations, so we'll leave it.
                        neighbor->active_family_tree (neighbors_offspring);

                        // Get all the neighbor's children that
                        // live on that side and are thus connected
                        // to us
                        for (const auto & child : neighbors_offspring)
                          {
                            // Skip neighbor offspring which are not in the range of elements being partitioned.
                            if (!global_index_map.count(child->id()))
                              continue;

                            // This does not assume a level-1 mesh.
                            // Note that since children have sides numbered
                            // coincident with the parent then this is a sufficient test.
                            if (child->neighbor_ptr(ns) == elem)
                              {
                                libmesh_assert (child->active());
                                num_neighbors++;
                              }
                          }
                      }

#endif /* ifdef LIBMESH_ENABLE_AMR */

                  }
              }

            // Check for any interior neighbors
            if ((elem->dim() < LIBMESH_DIM) && elem->interior_parent())
              {
                // get all relevant interior elements
                std::set<const Elem *> neighbor_set;
                elem->find_interior_neighbors(neighbor_set);

                num_neighbors += cast_int<unsigned int>(neighbor_set.size());
              }

            // Check for any boundary neighbors
            typedef map_type::iterator map_it_type;
            std::pair<map_it_type, map_it_type>
              bounds = interior_to_boundary_map.equal_range(elem);
            num_neighbors += cast_int<unsigned int>
              (std::distance(bounds.first, bounds.second));

            // Whether we enabled unique_id or had to fix any overlaps
            // by hand, our global indices should be unique, so we
            // should never see the same elem_global_index twice, so
            // we should never be trying to edit an already-nonzero
            // offset.
            libmesh_assert(!csr_graph.offsets[elem_global_index+1]);

            csr_graph.prep_n_nonzeros(elem_global_index, num_neighbors);
#ifndef NDEBUG
            graph_size += num_neighbors;
#endif
          }

        csr_graph.prepare_for_use();

        // (2) second pass - fill the compressed adjacency array
        for (const auto & elem : as_range(beg, end))
          {
            const dof_id_type elem_global_index =
              global_index_map[elem->id()];

            unsigned int connection=0;

            // Loop over the element's neighbors.  An element
            // adjacency corresponds to a face neighbor
            for (auto neighbor : elem->neighbor_ptr_range())
              {
                if (neighbor != nullptr)
                  {
                    // If the neighbor is active, but is not in the
                    // range of elements being partitioned, treat it
                    // as a nullptr neighbor.
                    if (neighbor->active() && !global_index_map.count(neighbor->id()))
                      continue;

                    // If the neighbor is active treat it
                    // as a connection
                    if (neighbor->active())
                      csr_graph(elem_global_index, connection++) = global_index_map[neighbor->id()];

#ifdef LIBMESH_ENABLE_AMR

                    // Otherwise we need to find all of the
                    // neighbor's children that are connected to
                    // us and add them
                    else
                      {
                        // The side of the neighbor to which
                        // we are connected
                        const unsigned int ns =
                          neighbor->which_neighbor_am_i (elem);
                        libmesh_assert_less (ns, neighbor->n_neighbors());

                        // Get all the active children (& grandchildren, etc...)
                        // of the neighbor.
                        neighbor->active_family_tree (neighbors_offspring);

                        // Get all the neighbor's children that
                        // live on that side and are thus connected
                        // to us
                        for (const auto & child : neighbors_offspring)
                          {
                            // Skip neighbor offspring which are not in the range of elements being partitioned.
                            if (!global_index_map.count(child->id()))
                              continue;

                            // This does not assume a level-1 mesh.
                            // Note that since children have sides numbered
                            // coincident with the parent then this is a sufficient test.
                            if (child->neighbor_ptr(ns) == elem)
                              {
                                libmesh_assert (child->active());

                                csr_graph(elem_global_index, connection++) = global_index_map[child->id()];
                              }
                          }
                      }

#endif /* ifdef LIBMESH_ENABLE_AMR */

                  }
              }

            if ((elem->dim() < LIBMESH_DIM) &&
                elem->interior_parent())
              {
                // get all relevant interior elements
                std::set<const Elem *> neighbor_set;
                elem->find_interior_neighbors(neighbor_set);

                for (const Elem * neighbor : neighbor_set)
                  {
                    // Not all interior neighbors are necessarily in
                    // the same Mesh (hence not in the global_index_map).
                    // This will be the case when partitioning a
                    // BoundaryMesh, whose elements all have
                    // interior_parents() that belong to some other
                    // Mesh.
                    const Elem * queried_elem = mesh.query_elem_ptr(neighbor->id());

                    // Compare the neighbor and the queried_elem
                    // pointers, make sure they are the same.
                    if (queried_elem && queried_elem == neighbor)
                      csr_graph(elem_global_index, connection++) =
                        libmesh_map_find(global_index_map, neighbor->id());
                  }
              }

            // Check for any boundary neighbors
            for (const auto & pr : as_range(interior_to_boundary_map.equal_range(elem)))
              {
                const Elem * neighbor = pr.second;
                csr_graph(elem_global_index, connection++) =
                  global_index_map[neighbor->id()];
              }
          }

        // We create a non-empty vals for a disconnected graph, to
        // work around a segfault from METIS.
        libmesh_assert_equal_to (csr_graph.vals.size(),
                                 std::max(graph_size, std::size_t(1)));
      } // done building the graph

      Metis::idx_t ncon = 1;

      // Select which type of partitioning to create

      // Use recursive if the number of partitions is less than or equal to 8
      if (n_pieces <= 8)
        Metis::METIS_PartGraphRecursive(&n,
                                        &ncon,
                                        csr_graph.offsets.data(),
                                        csr_graph.vals.data(),
                                        vwgt.data(),
                                        nullptr,
                                        nullptr,
                                        &nparts,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        &edgecut,
                                        part.data());

      // Otherwise  use kway
      else
        Metis::METIS_PartGraphKway(&n,
                                   &ncon,
                                   csr_graph.offsets.data(),
                                   csr_graph.vals.data(),
                                   vwgt.data(),
                                   nullptr,
                                   nullptr,
                                   &nparts,
                                   nullptr,
                                   nullptr,
                                   nullptr,
                                   &edgecut,
                                   part.data());

    } // end processor 0 part

  // Broadcast the resulting partition
  mesh.comm().broadcast(part);

  // Assign the returned processor ids.  The part array contains
  // the processor id for each active element, but in terms of
  // the contiguous indexing we defined above
  for (auto & elem : as_range(beg, end))
    {
      libmesh_assert (global_index_map.count(elem->id()));

      const dof_id_type elem_global_index =
        global_index_map[elem->id()];

      libmesh_assert_less (elem_global_index, part.size());
      const processor_id_type elem_procid =
        static_cast<processor_id_type>(part[elem_global_index]);

      elem->processor_id() = elem_procid;
    }
#endif
}



void MetisPartitioner::_do_partition (MeshBase & mesh,
                                      const unsigned int n_pieces)
{
  this->partition_range(mesh,
                        mesh.active_elements_begin(),
                        mesh.active_elements_end(),
                        n_pieces);
}

} // namespace libMesh
