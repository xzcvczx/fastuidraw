/*!
 * \file painter_attribute_data_filler_path_stroked.cpp
 * \brief file painter_attribute_data_filler_path_stroked.cpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@intel.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */

#include <algorithm>
#include <fastuidraw/painter/painter_attribute_data_filler_path_stroked.hpp>

namespace
{

  /*!
    Enumation values are indexes into attribute_data_chunks()
    and index_data_chunks() for different portions of
    data needed for stroking a path when the data of this
    PainterAttributeData has been set with
    set_data(const reference_counted_ptr<const StrokedPath> &).
  */
  enum stroking_data_t
    {
      rounded_joins_closing_edge, /*!< index for rounded join data with closing edge */
      bevel_joins_closing_edge, /*!< index for bevel join data with closing edge */
      miter_joins_closing_edge, /*!< index for miter join data with closing edge */
      cap_joins_closing_edge, /*!< index for cap-join data with closing edge */
      edge_closing_edge, /*!< index for edge data including closing edge */

      number_with_closing_edge, /*!< number of types with closing edge */

      rounded_joins_no_closing_edge = number_with_closing_edge, /*!< index for rounded join data without closing edge */
      bevel_joins_no_closing_edge, /*!< index for bevel join data without closing edge */
      miter_joins_no_closing_edge, /*!< index for miter join data without closing edge */
      cap_joins_no_closing_edge, /*!< index for cap-join data without closing edge */
      edge_no_closing_edge, /*!< index for edge data not including closing edge */

      rounded_cap, /*!< index for rounded cap data */
      square_cap,  /*!< index for square cap data */
      adjustable_cap,

      /*!
        count of enums, using this enumeration when on data created
        from a StrokedPath, gives empty indices and attributes.
      */
      stroking_data_count
    };

  enum stroking_data_t
  without_closing_edge(enum stroking_data_t v)
  {
    return (v < number_with_closing_edge) ?
      static_cast<enum stroking_data_t>(v + number_with_closing_edge) :
      v;
  }

  class ChunkSelector:public fastuidraw::StrokingChunkSelectorBase
  {
  public:
    static
    enum stroking_data_t
    static_cap_chunk(enum fastuidraw::PainterEnums::cap_style cp);

    static
    enum stroking_data_t
    static_edge_chunk(bool edge_closed);

    static
    enum stroking_data_t
    static_join_chunk(enum fastuidraw::PainterEnums::join_style js, bool edge_closed);

    static
    unsigned int
    static_named_join_chunk(enum stroking_data_t join, unsigned int J);

    static
    unsigned int
    static_adjustable_cap_chunk(void)
    {
      return adjustable_cap;
    }

    virtual
    unsigned int
    cap_chunk(enum fastuidraw::PainterEnums::cap_style cp) const
    {
      return static_cap_chunk(cp);
    }

    virtual
    unsigned int
    adjustable_cap_chunk(void) const
    {
      return static_adjustable_cap_chunk();
    }

    virtual
    unsigned int
    edge_chunk(bool edge_closed) const
    {
      return static_edge_chunk(edge_closed);
    }

    virtual
    unsigned int
    join_chunk(enum fastuidraw::PainterEnums::join_style js, bool edge_closed) const
    {
      return static_join_chunk(js, edge_closed);
    }

    virtual
    unsigned int
    named_join_chunk(enum fastuidraw::PainterEnums::join_style js, unsigned int J) const
    {
      enum stroking_data_t join;
      join = static_join_chunk(js, true);
      return static_named_join_chunk(join, J);
    }

    virtual
    unsigned int
    chunk_from_cap_join(unsigned int J) const
    {
      return static_named_join_chunk(cap_joins_closing_edge, J);
    }
  };

  class PathStrokerPrivate
  {
  public:
    enum { number_join_types = 4 };

    /*!
      Given an enumeration of stroking_data_t, returns
      the matching enumeration for drawing without the
      closing edge.
     */
    static
    enum stroking_data_t
    without_closing_edge(enum stroking_data_t v);


    PathStrokerPrivate(const fastuidraw::reference_counted_ptr<const fastuidraw::StrokedPath> &p):
      m_path(p)
    {}

    fastuidraw::reference_counted_ptr<const fastuidraw::StrokedPath> m_path;
  };

  void
  grab_attribute_index_data(fastuidraw::c_array<fastuidraw::PainterAttribute> attrib_dst,
                            unsigned int &attr_loc,
                            fastuidraw::const_c_array<fastuidraw::StrokedPath::point> attr_src,
                            fastuidraw::c_array<fastuidraw::PainterIndex> idx_dst, unsigned int &idx_loc,
                            fastuidraw::const_c_array<unsigned int> idx_src,
                            fastuidraw::const_c_array<fastuidraw::PainterAttribute> &attrib_chunks,
                            fastuidraw::const_c_array<fastuidraw::PainterIndex> &index_chunks)
  {
    attrib_dst = attrib_dst.sub_array(attr_loc, attr_src.size());
    idx_dst = idx_dst.sub_array(idx_loc, idx_src.size());

    attrib_chunks = attrib_dst;
    index_chunks = idx_dst;

    std::copy(idx_src.begin(), idx_src.end(), idx_dst.begin());
    std::transform(attr_src.begin(), attr_src.end(), attrib_dst.begin(),
                   fastuidraw::PainterAttributeDataFillerPathStroked::pack_point);

    attr_loc += attr_src.size();
    idx_loc += idx_src.size();
  }
}

/////////////////////////////////////////////////////
// ChunkSelector methods
enum stroking_data_t
ChunkSelector::
static_cap_chunk(enum fastuidraw::PainterEnums::cap_style cp)
{
  using namespace fastuidraw::PainterEnums;
  enum stroking_data_t cap;

  switch(cp)
    {
    case rounded_caps:
      cap = rounded_cap;
      break;
    case square_caps:
      cap = square_cap;
      break;
    default:
      cap = stroking_data_count;
    }
  return cap;
}

enum stroking_data_t
ChunkSelector::
static_edge_chunk(bool edge_closed)
{
  return edge_closed ?
    edge_closing_edge :
    edge_no_closing_edge;
}

enum stroking_data_t
ChunkSelector::
static_join_chunk(enum fastuidraw::PainterEnums::join_style js, bool edge_closed)
{
  using namespace fastuidraw::PainterEnums;
  enum stroking_data_t join;

  switch(js)
    {
    case rounded_joins:
      join = rounded_joins_closing_edge;
      break;
    case bevel_joins:
      join = bevel_joins_closing_edge;
      break;
    case miter_joins:
      join = miter_joins_closing_edge;
      break;
    default:
      join = stroking_data_count;
    }

  if(!edge_closed)
    {
      join = without_closing_edge(join);
    }
  return join;
}

unsigned int
ChunkSelector::
static_named_join_chunk(enum stroking_data_t join, unsigned int J)
{
  /*
    There are number_join_types joins, the chunk
    for the J'th join for type tp is located
    at number_join_types * J + t + stroking_data_count
    where 0 <=t < number_join_types is derived from tp.
   */
  unsigned int t;
  switch(join)
    {
    case rounded_joins_closing_edge:
    case rounded_joins_no_closing_edge:
      t = 0u;
      break;

    case bevel_joins_closing_edge:
    case bevel_joins_no_closing_edge:
      t = 1u;
      break;

    case miter_joins_closing_edge:
    case miter_joins_no_closing_edge:
      t = 2u;
      break;

    case cap_joins_closing_edge:
    case cap_joins_no_closing_edge:
      t = 3u;
      break;

    default:
      assert(!"Type not mapping to join type");
      t = 0u;
    }

  /* The +1 is so that the chunk at stroking_data_count
     is left as empty for PainterAttributeData set
     from a StrokedPath (Painter relies on this!).
   */
  return stroking_data_count + t + 1
    + J * PathStrokerPrivate::number_join_types;
}

//////////////////////////////////////////////////////////
// fastuidraw::PainterAttributeDataFillerPathStroked methods
fastuidraw::PainterAttributeDataFillerPathStroked::
PainterAttributeDataFillerPathStroked(const reference_counted_ptr<const StrokedPath> &path)
{
  m_d = FASTUIDRAWnew PathStrokerPrivate(path);
}

fastuidraw::PainterAttributeDataFillerPathStroked::
~PainterAttributeDataFillerPathStroked()
{
  PathStrokerPrivate *d;
  d = reinterpret_cast<PathStrokerPrivate*>(m_d);
  FASTUIDRAWdelete(d);
  m_d = NULL;
}

const fastuidraw::reference_counted_ptr<const fastuidraw::StrokedPath>&
fastuidraw::PainterAttributeDataFillerPathStroked::
path(void) const
{
  PathStrokerPrivate *d;
  d = reinterpret_cast<PathStrokerPrivate*>(m_d);
  assert(d != NULL);
  return d->m_path;
}

void
fastuidraw::PainterAttributeDataFillerPathStroked::
compute_sizes(unsigned int &num_attributes,
              unsigned int &num_indices,
              unsigned int &num_attribute_chunks,
              unsigned int &num_index_chunks,
              unsigned int &number_z_increments) const
{
  const StrokedPath *p(path().get());

  if(p == NULL)
    {
      return;
    }

  unsigned int numJoins(0);
  for(unsigned int i = 0; i < StrokedPath::number_point_set_types; ++i)
    {
      enum StrokedPath::point_set_t tp;
      tp = static_cast<enum StrokedPath::point_set_t>(i);
      num_attributes += p->points(tp, true).size();
      num_indices += p->indices(tp, true).size();
    }

  for(unsigned int C = 0, endC = p->number_contours(); C < endC; ++C)
    {
      unsigned int endJ;

      endJ = p->number_joins(C);
      numJoins += endJ;
      for(unsigned int J = 0; J < endJ; ++J)
        {
          /* indices for joins appear twice, once all together
             and then again as seperate chunks.
           */
          num_indices += p->indices_range(StrokedPath::bevel_join_point_set, C, J).difference();
          num_indices += p->indices_range(StrokedPath::rounded_join_point_set, C, J).difference();
          num_indices += p->indices_range(StrokedPath::miter_join_point_set, C, J).difference();
          num_indices += p->indices_range(StrokedPath::cap_join_point_set, C, J).difference();
        }
    }
  num_attribute_chunks = stroking_data_count + 1 + PathStrokerPrivate::number_join_types * numJoins;
  num_index_chunks = num_attribute_chunks;
  number_z_increments = stroking_data_count;
}


void
fastuidraw::PainterAttributeDataFillerPathStroked::
fill_data(c_array<PainterAttribute> attribute_data,
          c_array<PainterIndex> index_data,
          c_array<const_c_array<PainterAttribute> > attribute_chunks,
          c_array<const_c_array<PainterIndex> > index_chunks,
          c_array<unsigned int> zincrements) const
{
  const StrokedPath *p(path().get());

  if(p == NULL)
    {
      return;
    }

  unsigned int attr_loc(0), idx_loc(0);

  zincrements[edge_closing_edge] = p->number_depth(StrokedPath::edge_point_set, true);
  zincrements[bevel_joins_closing_edge] = p->number_depth(StrokedPath::bevel_join_point_set, true);
  zincrements[rounded_joins_closing_edge] = p->number_depth(StrokedPath::rounded_join_point_set, true);
  zincrements[miter_joins_closing_edge] = p->number_depth(StrokedPath::miter_join_point_set, true);
  zincrements[cap_joins_closing_edge] = p->number_depth(StrokedPath::cap_join_point_set, true);

  zincrements[edge_no_closing_edge] = p->number_depth(StrokedPath::edge_point_set, false);
  zincrements[bevel_joins_no_closing_edge] = p->number_depth(StrokedPath::bevel_join_point_set, false);
  zincrements[rounded_joins_no_closing_edge] = p->number_depth(StrokedPath::rounded_join_point_set, false);
  zincrements[miter_joins_no_closing_edge] = p->number_depth(StrokedPath::miter_join_point_set, false);
  zincrements[cap_joins_no_closing_edge] = p->number_depth(StrokedPath::cap_join_point_set, false);

  zincrements[square_cap] = p->number_depth(StrokedPath::square_cap_point_set, false);
  zincrements[rounded_cap] = p->number_depth(StrokedPath::rounded_cap_point_set, false);

  grab_attribute_index_data(attribute_data, attr_loc,
                            p->points(StrokedPath::square_cap_point_set, false),
                            index_data, idx_loc,
                            p->indices(StrokedPath::square_cap_point_set, false),
                            attribute_chunks[square_cap], index_chunks[square_cap]);

  grab_attribute_index_data(attribute_data, attr_loc,
                            p->points(StrokedPath::rounded_cap_point_set, false),
                            index_data, idx_loc,
                            p->indices(StrokedPath::rounded_cap_point_set, false),
                            attribute_chunks[rounded_cap], index_chunks[rounded_cap]);

  grab_attribute_index_data(attribute_data, attr_loc,
                            p->points(StrokedPath::adjustable_cap_point_set, false),
                            index_data, idx_loc,
                            p->indices(StrokedPath::adjustable_cap_point_set, false),
                            attribute_chunks[adjustable_cap], index_chunks[adjustable_cap]);

#define GRAB_MACRO(dst_root_name, src_name) do {                        \
                                                                        \
    enum stroking_data_t closing_edge = dst_root_name##_closing_edge;   \
    enum stroking_data_t no_closing_edge = dst_root_name##_no_closing_edge; \
                                                                        \
    grab_attribute_index_data(attribute_data,  attr_loc,                \
                              p->points(src_name, true),                \
                              index_data, idx_loc,                      \
                              p->indices(src_name, true),               \
                              attribute_chunks[closing_edge],           \
                              index_chunks[closing_edge]);              \
    attribute_chunks[no_closing_edge] =                                 \
      attribute_chunks[closing_edge].sub_array(0, p->points(src_name, false).size()); \
                                                                        \
    unsigned int with, without;                                         \
    with = p->indices(src_name, true).size();                           \
    without = p->indices(src_name, false).size();                       \
    assert(with >= without);                                            \
                                                                        \
    index_chunks[no_closing_edge] =                                     \
      index_chunks[closing_edge].sub_array(with - without);             \
  } while(0)

#define GRAB_JOIN_MACRO_HELPER(src_name, closing_edge, C, J, gJ) do {   \
    unsigned int Kc;                                                    \
    range_type<unsigned int> Ri, Ra;                                    \
    const_c_array<unsigned int> srcI;                                   \
    c_array<unsigned int> dst;                                          \
                                                                        \
    Ra = p->points_range(src_name, C, J);                               \
    Ri = p->indices_range(src_name, C, J);                              \
    srcI = p->indices(src_name, true).sub_array(Ri);                    \
    Kc = ChunkSelector::static_named_join_chunk(closing_edge, gJ);      \
    attribute_chunks[Kc] = attribute_chunks[closing_edge].sub_array(Ra); \
    dst = index_data.sub_array(idx_loc, srcI.size());                   \
    index_chunks[Kc] =  dst;                                            \
    for(unsigned int I = 0; I < srcI.size(); ++I)                       \
      {                                                                 \
        dst[I] = srcI[I] - Ra.m_begin;                                  \
      }                                                                 \
    idx_loc += srcI.size();                                             \
    ++gJ;                                                               \
  } while(0)

  /* note that the last two joins of each contour are grabbed last,
     this is to make sure that the joins for the closing edge are
     always at the end of our join list.
   */
#define GRAB_JOIN_MACRO(dst_root_name, src_name) do {                   \
    enum stroking_data_t closing_edge = dst_root_name##_closing_edge;   \
    unsigned int gJ = 0;                                                \
    for(unsigned int C = 0, endC = p->number_contours();                \
        C < endC; ++C)                                                  \
      {                                                                 \
        for(unsigned int J = 0, endJ = p->number_joins(C) - 2;          \
            J < endJ; ++J)                                              \
          {                                                             \
            GRAB_JOIN_MACRO_HELPER(src_name, closing_edge, C, J, gJ);   \
          }                                                             \
      }                                                                 \
    for(unsigned int C = 0, endC = p->number_contours();                \
        C < endC; ++C)                                                  \
      {                                                                 \
        if(p->number_joins(C) >= 2)                                     \
          {                                                             \
            unsigned int J;                                             \
            J = p->number_joins(C) - 1;                                 \
            GRAB_JOIN_MACRO_HELPER(src_name, closing_edge, C, J, gJ);   \
            J = p->number_joins(C) - 2;                                 \
            GRAB_JOIN_MACRO_HELPER(src_name, closing_edge, C, J, gJ);   \
          }                                                             \
      }                                                                 \
  } while(0)

  //grab the data for edges and joint together
  GRAB_MACRO(edge, StrokedPath::edge_point_set);
  GRAB_MACRO(bevel_joins, StrokedPath::bevel_join_point_set);
  GRAB_MACRO(rounded_joins, StrokedPath::rounded_join_point_set);
  GRAB_MACRO(miter_joins, StrokedPath::miter_join_point_set);
  GRAB_MACRO(cap_joins, StrokedPath::cap_join_point_set);

  // then grab individual joins, this must be done after
  // all the blocks because although it does not generate new
  // attribute data, but it does generate new index data
  GRAB_JOIN_MACRO(rounded_joins, StrokedPath::rounded_join_point_set);
  GRAB_JOIN_MACRO(bevel_joins, StrokedPath::bevel_join_point_set);
  GRAB_JOIN_MACRO(miter_joins, StrokedPath::miter_join_point_set);
  GRAB_JOIN_MACRO(cap_joins, StrokedPath::cap_join_point_set);
}


fastuidraw::reference_counted_ptr<fastuidraw::StrokingChunkSelectorBase>
fastuidraw::PainterAttributeDataFillerPathStroked::
chunk_selector(void)
{
  return FASTUIDRAWnew ChunkSelector();
}


fastuidraw::StrokedPath::point
fastuidraw::PainterAttributeDataFillerPathStroked::
unpack_point(const PainterAttribute &a)
{
  StrokedPath::point dst;

  dst.m_position.x() = unpack_float(a.m_attrib0.x());
  dst.m_position.y() = unpack_float(a.m_attrib0.y());

  dst.m_pre_offset.x() = unpack_float(a.m_attrib0.z());
  dst.m_pre_offset.y() = unpack_float(a.m_attrib0.w());

  dst.m_distance_from_edge_start = unpack_float(a.m_attrib1.x());
  dst.m_distance_from_contour_start = unpack_float(a.m_attrib1.y());
  dst.m_auxilary_offset.x() = unpack_float(a.m_attrib1.z());
  dst.m_auxilary_offset.y() = unpack_float(a.m_attrib1.w());

  dst.m_packed_data = a.m_attrib2.x();
  dst.m_edge_length = unpack_float(a.m_attrib2.y());
  dst.m_open_contour_length = unpack_float(a.m_attrib2.z());
  dst.m_closed_contour_length = unpack_float(a.m_attrib2.w());

  return dst;
}

fastuidraw::PainterAttribute
fastuidraw::PainterAttributeDataFillerPathStroked::
pack_point(const StrokedPath::point &src)
{
  PainterAttribute dst;

  dst.m_attrib0 = pack_vec4(src.m_position.x(),
                            src.m_position.y(),
                            src.m_pre_offset.x(),
                            src.m_pre_offset.y());

  dst.m_attrib1 = pack_vec4(src.m_distance_from_edge_start,
                            src.m_distance_from_contour_start,
                            src.m_auxilary_offset.x(),
                            src.m_auxilary_offset.y());

  dst.m_attrib2 = uvec4(src.m_packed_data,
                        pack_float(src.m_edge_length),
                        pack_float(src.m_open_contour_length),
                        pack_float(src.m_closed_contour_length));

  return dst;
}
