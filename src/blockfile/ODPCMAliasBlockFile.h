/**********************************************************************

  Audacity: A Digital Audio Editor

  ODPCMAliasBlockFile.cpp

  Created by Michael Chinen (mchinen)
  Audacity(R) is copyright (c) 1999-2008 Audacity Team.
  License: GPL v2.  See License.txt.

******************************************************************//**

\class ODPCMAliasBlockFile
\brief ODPCMAliasBlockFile is a special type of PCMAliasBlockFile that does not necessarily have summary data available
The summary is eventually computed and written to a file in a background thread.

Load On-Demand implementation of the AliasBlockFile for PCM files.

to load large files more quickly, we take skip computing the summary data and put
ODPCMAliasBlockFiles in the sequence as place holders.  A background thread loads and
computes the summary data into these classes.
ODPCMAliasBlockFiles are unlike all other BlockFiles are not immutable (for the most part,) because when NEW
summary data is computed for an existing ODPCMAliasBlockFile we save the buffer then and write the Summary File.

All BlockFile methods that treat the summary data as a buffer that exists in its BlockFile
are implemented here to behave when the data is not available yet.

Some of these methods have been overridden only because they used the unsafe wxLog calls in the base class.
*//*******************************************************************/






#ifndef __AUDACITY_ODPCMALIASBLOCKFILE__
#define __AUDACITY_ODPCMALIASBLOCKFILE__

#include "PCMAliasBlockFile.h"
#include "../BlockFile.h"
#include "../ondemand/ODTaskThread.h"
#include "../DirManager.h"
#include <wx/thread.h>

/// An AliasBlockFile that references uncompressed data in an existing file
class ODPCMAliasBlockFile final : public PCMAliasBlockFile
{
 public:
   /// Constructs a PCMAliasBlockFile, writing the summary to disk
   ODPCMAliasBlockFile(wxFileNameWrapper &&baseFileName,
                        wxFileNameWrapper &&aliasedFileName, sampleCount aliasStart,
                        sampleCount aliasLen, int aliasChannel);
   ODPCMAliasBlockFile(wxFileNameWrapper &&existingSummaryFileName,
                        wxFileNameWrapper &&aliasedFileName, sampleCount aliasStart,
                        sampleCount aliasLen, int aliasChannel,
                        float min, float max, float rms, bool summaryAvailable);
   virtual ~ODPCMAliasBlockFile();

   //checks to see if summary data has been computed and written to disk yet.  Thread safe.  Blocks if we are writing summary data.
   bool IsSummaryAvailable() const override;

   /// Returns TRUE if the summary has not yet been written, but is actively being computed and written to disk
   bool IsSummaryBeingComputed() override { return mSummaryBeingComputed; }

   //Calls that rely on summary files need to be overidden
   wxLongLong GetSpaceUsage() const override;
   /// Gets extreme values for the specified region
   void GetMinMax(sampleCount start, sampleCount len,
                          float *outMin, float *outMax, float *outRMS) const override;
   /// Gets extreme values for the entire block
   void GetMinMax(float *outMin, float *outMax, float *outRMS) const override;
   /// Returns the 256 byte summary data block
   bool Read256(float *buffer, sampleCount start, sampleCount len) override;
   /// Returns the 64K summary data block
   bool Read64K(float *buffer, sampleCount start, sampleCount len) override;

   ///Makes NEW ODPCMAliasBlockFile or PCMAliasBlockFile depending on summary availability
   BlockFile *Copy(wxFileNameWrapper &&fileName) override;

   ///Saves as xml ODPCMAliasBlockFile or PCMAliasBlockFile depending on summary availability
   void SaveXML(XMLWriter &xmlFile) override;

   ///Reconstructs from XML a ODPCMAliasBlockFile and reschedules it for OD loading
   static BlockFile *BuildFromXML(DirManager &dm, const wxChar **attrs);

   ///Writes the summary file if summary data is available
   void Recover(void) override;

   ///A public interface to WriteSummary
   void DoWriteSummary();

   ///Sets the value that indicates where the first sample in this block corresponds to the global sequence/clip.  Only for display use.
   void SetStart(sampleCount startSample){mStart = startSample;}

   ///Gets the value that indicates where the first sample in this block corresponds to the global sequence/clip.  Only for display use.
   sampleCount GetStart() const {return mStart;}

   /// Locks the blockfile only if it has a file that exists.
   void Lock();

   /// Unlocks the blockfile only if it has a file that exists.
   void Unlock();

   ///sets the amount of samples the clip associated with this blockfile is offset in the wavetrack (non effecting)
   void SetClipOffset(sampleCount numSamples){mClipOffset= numSamples;}

   ///Gets the number of samples the clip associated with this blockfile is offset by.
   sampleCount GetClipOffset() const {return mClipOffset;}

   //returns the number of samples from the beginning of the track that this blockfile starts at
   sampleCount GetGlobalStart() const {return mClipOffset+mStart;}

   //returns the number of samples from the beginning of the track that this blockfile ends at
   sampleCount GetGlobalEnd() const {return mClipOffset+mStart+GetLength();}


   //Below calls are overrided just so we can take wxlog calls out, which are not threadsafe.

   /// Reads the specified data from the aliased file using libsndfile
   int ReadData(samplePtr data, sampleFormat format,
                        sampleCount start, sampleCount len) const override;

   /// Read the summary into a buffer
   bool ReadSummary(void *data) override;

   ///sets the file name the summary info will be saved in.  threadsafe.
   void SetFileName(wxFileNameWrapper &&name) override;
   GetFileNameResult GetFileName() const override;

   //when the file closes, it locks the blockfiles, but only conditionally.
   // It calls this so we can check if it has been saved before.
   // not balanced by unlocking calls.
   void CloseLock() override;

   /// Prevents a read on other threads.
   void LockRead() const override;
   /// Allows reading on other threads.
   void UnlockRead() const override;

protected:
   void WriteSummary() override;
   void *CalcSummary(samplePtr buffer, sampleCount len,
      sampleFormat format, ArrayOf<char> &cleanup) override;

  private:
   //Thread-safe versions
   void Ref() const override;
   bool Deref() const override;
   //needed for Ref/Deref access.
   friend class DirManager;
   friend class ODComputeSummaryTask;
   friend class ODDecodeTask;

   ODLock mWriteSummaryMutex;

   //need to protect this since it is changed from the main thread upon save.
   mutable ODLock mFileNameMutex;

   ///Also need to protect the aliased file name.
   ODLock mAliasedFileNameMutex;

   //lock the read data - libsndfile can't handle two reads at once?
   mutable ODLock mReadDataMutex;


   //lock the Ref counting
   mutable ODLock mDerefMutex;
   mutable ODLock mRefMutex;

   mutable ODLock    mSummaryAvailableMutex;
   bool mSummaryAvailable;
   bool mSummaryBeingComputed;
   bool mHasBeenSaved;

   ///for reporting after task is complete.  Only for display use.
   sampleCount mStart;

   ///the ODTask needs to know where this blockfile lies in the track, so for convenience, we have this here.
   sampleCount mClipOffset;
};

#endif

