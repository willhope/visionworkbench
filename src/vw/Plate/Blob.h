// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


#ifndef __VW_PLATE_BLOBIO__
#define __VW_PLATE_BLOBIO__

/// \file Blob.h
///
/// Platefile data is stored in data "blobs" that exist as files on the
/// filesystem.  New data is added to the data blobs whenever a new tile
/// is written to the platefile.  To ensure data consistency (as one would
/// have in a journaling filesystem), the new data is first written to a
/// journal file (a sidecar that exists alongside the blob), and then it
/// is copied from the journal file to the blob file.  In this way, the
/// data will always be written to the blob if it is succesfully written
/// to the journal, and if the data is not successfully written to the
/// journal, then it does not end up corrupting the blob. (The actual
/// process is considerably more complicated and optimized, but this is
/// the general idea.)
///
/// Data in the blob is stored in stanzas with the following way:
///
///   [ BLOB HEADER_SIZE ]  [ uint16 ]
///   [ BLOB HEADER ]       [ uint8 - serialized BlobHeader protobuffer ]
///     (contains HEADER_OFFSET, DATA_OFFSET, HEADER_SIZE, DATA_SIZE)
///
///   [ HEADER ]            [ uint8 - serialized IndexRecord protobuffer ]
///
///   [ DATA ]              [ uint8 - N raw bytes of data ]
///
///

#include <vw/Plate/IndexData.pb.h>
#include <vw/Core/Exception.h>
#include <vw/Core/FundamentalTypes.h>
#include <vw/Core/Log.h>
#include <boost/shared_array.hpp>
#include <fstream>
#include <string>

namespace vw {
namespace platefile {

  // -------------------------------------------------------------------
  //                                 BLOB
  // -------------------------------------------------------------------

  class Blob : boost::noncopyable {

    std::string m_blob_filename;
    boost::shared_ptr<std::fstream> m_fstream;
    uint64 m_end_of_file_ptr;
    uint64 m_write_count;

    /// Returns the metadata (i.e. BlobRecord) for a blob entry.
    BlobRecord read_blob_record(uint16 &blob_record_size) const;

    // End-of-file point manipulation.
    void write_end_of_file_ptr(uint64 ptr);
    uint64 read_end_of_file_ptr() const;

  public:

    // ------------------------ Blob Iterator ------------------------------

    /// An STL-compliant iterator for iterating over the index entries
    /// in a blob.  This can be used by Index.h to read in and rebuild
    /// the Index tree.
    class iterator : public boost::iterator_facade<Blob::iterator, TileHeader,
                                                   boost::forward_traversal_tag,
                                                   TileHeader, int64> {

      // This is required for boost::iterator_facade
      friend class boost::iterator_core_access;

      // Private variables
      Blob& m_blob;
      uint64 m_current_base_offset;

      // Iterator methods.  The boost iterator facade takes these and
      // uses them to construct normal iterator methods.
      bool equal (iterator const& iter) const {
        return (m_current_base_offset == iter.m_current_base_offset);
      }

      void increment() {
        m_current_base_offset = m_blob.next_base_offset(m_current_base_offset);
      }

      TileHeader const dereference() const {
        return m_blob.read_header(m_current_base_offset);
      }

    public:

      // Constructors
      iterator( Blob &blob, uint64 base_offset )
        : m_blob(blob), m_current_base_offset(base_offset) {}

      uint64 current_base_offset() const { return m_current_base_offset; }
      uint64 current_data_size() const { return m_blob.data_size(m_current_base_offset); }

    };

    // -----------------------------------------------------------------------

    /// Constructor
    Blob(std::string filename, bool readonly = false);

    /// The destructor flushes any unwritten journal entries and
    /// closes the blob and journal files.
    ~Blob();

    /// Returns the size of the blob in bytes.  Note: only counts
    /// valid entries.  (Invalid data may exist beyond the end of the
    /// end_of_file_ptr)
    uint64 size() const { return m_end_of_file_ptr; }

    /// Returns an iterator pointing to the first TileHeader in the blob.
    ///
    /// 3*sizeof(uint64) is the very first byte in the file after the
    /// end-of-file pointer.  (See the *_end_of_file_ptr() routines
    /// above for more info...)
    iterator begin() { return iterator(*this, 3*sizeof(uint64) ); }

    /// Returns an iterator pointing one past the last TileHeader in the blob.
    iterator end() { return iterator(*this, m_end_of_file_ptr ); }

    /// Seek to the next base offset given the current base offset.
    uint64 next_base_offset(uint64 current_base_offset);

    /// Returns binary index record (a serialized protobuffer) for an
    /// entry starting at base_offset.
    TileHeader read_header(vw::uint64 base_offset64);

    /// Returns the binary data for an entry starting at base_offset.
    boost::shared_array<uint8> read_data(vw::uint64 base_offset, vw::uint64& data_size);

    /// Returns the parameters necessary to call sendfile(2)
    void read_sendfile(vw::uint64 base_offset, std::string& filename, vw::uint64& offset, vw::uint64& size);

    /// Returns the data size
    uint64 data_size(uint64 base_offset) const;

    /// Write a tile to the blob file. You must supply the header
    /// (e.g. a serialized TileHeader protobuffer) and the data as
    /// shared_arrays.  Returns the base_offset where the data was
    /// written to the blob file.
    vw::uint64 write(TileHeader const& header, boost::shared_array<uint8> data, uint64 data_size);

    /// Read data out of the blob and save it as its own file on disk.
    void read_to_file(std::string dest_file, vw::uint64 offset);

    /// Write the data file to disk, and the concatenate it into the data blob.
    void write_from_file(std::string source_file, TileHeader const& header, uint64& base_offset);

  };

}} // namespace vw::platefile

#endif // __VW_PLATE_BLOBIO__
