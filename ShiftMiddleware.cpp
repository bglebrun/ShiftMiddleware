#include <windows.h>
#include "sharedmemory.h"
#include <stdio.h>
#include <conio.h>
#include "DirectInput.h"
#define MAP_OBJECT_NAME "$pcars2$"
// Keybinds to use for shifting, see: https://docs.microsoft.com/en-us/windows/desktop/inputdev/virtual-key-codes
// Default O->ShiftDown P->ShiftUp
/*
#define SHIFT_UP_KEYBIND 0x50
#define SHIFT_DOWN_KEYBIND 0x4F
*/

int main() {
  // Keyboard input event
  INPUT ip;
  // Setup INPUT
  ip.type = INPUT_KEYBOARD;
  ip.ki.time = 0;
  ip.ki.wVk = 0;
  ip.ki.dwExtraInfo = 0;
  ip.ki.dwFlags = KEYEVENTF_SCANCODE;

  // Change these in fomat DIKEYBOARD_*, see: https://docs.microsoft.com/en-us/previous-versions/windows/desktop/ee418643(v=vs.85)
  /*
  RegToDirect[SHIFT_UP_KEYBIND] = DIKEYBOARD_P;
  RegToDirect[SHIFT_DOWN_KEYBIND] = DIKEYBOARD_O;
  */
  // Open the memory-mapped file
  HANDLE fileHandle = OpenFileMapping( PAGE_READONLY, FALSE, MAP_OBJECT_NAME );
  if (fileHandle == NULL)
  {
    printf( "Could not open file mapping object (%d).\n", GetLastError() );
    return 1;
  }

  // Get the data structure
  const SharedMemory* sharedData = (SharedMemory*)MapViewOfFile( fileHandle, PAGE_READONLY, 0, 0, sizeof(SharedMemory) );
  SharedMemory* localCopy = new SharedMemory;
  if (sharedData == NULL)
  {
    printf( "Could not map view of file (%d).\n", GetLastError() );

    CloseHandle( fileHandle );
    return 1;
  }

  // Ensure we're sync'd to the correct data version
  if ( sharedData->mVersion != SHARED_MEMORY_VERSION )
  {
    printf( "Data version mismatch! Need %d, have %d\n", sharedData->mVersion, SHARED_MEMORY_VERSION);
    return 1;
  }


  //------------------------------------------------------------------------------
  // TEST DISPLAY CODE
  //------------------------------------------------------------------------------
  unsigned int updateIndex(0);
  unsigned int indexChange(0);
  printf( "ESC TO EXIT\n\n" );
  while (true)
  {
    if ( sharedData->mSequenceNumber % 2 )
    {
      // Odd sequence number indicates, that write into the shared memory is just happening
      continue;
    }

    indexChange = sharedData->mSequenceNumber - updateIndex;
    updateIndex = sharedData->mSequenceNumber;

    //Copy the whole structure before processing it, otherwise the risk of the game writing into it during processing is too high.
    memcpy(localCopy,sharedData,sizeof(SharedMemory));


    if (localCopy->mSequenceNumber != updateIndex )
    {
      // More writes had happened during the read. Should be rare, but can happen.
      continue;
    }

    printf( "Sequence number increase %d, current index %d, previous index %d\n", indexChange, localCopy->mSequenceNumber, updateIndex );

    const bool isValidParticipantIndex = localCopy->mViewedParticipantIndex != -1 && localCopy->mViewedParticipantIndex < localCopy->mNumParticipants && localCopy->mViewedParticipantIndex < STORED_PARTICIPANTS_MAX;
    if ( isValidParticipantIndex )
    {
      const ParticipantInfo& viewedParticipantInfo = localCopy->mParticipantInfo[sharedData->mViewedParticipantIndex];
      printf( "mParticipantName: (%s)\n", viewedParticipantInfo.mName );
      printf( "lap Distance = %f \n", viewedParticipantInfo.mCurrentLapDistance );
    }

    /*
    In final implementation, this is where our UART communication code would go
    */
    printf( "RPM: %0.2f / %0.2f\n", localCopy->mRpm, localCopy->mMaxRPM );
    printf( "GEAR: %d / %d\n", localCopy->mGear, localCopy->mNumGears );
    printf( "SPEED: %0.2fd \n", localCopy->mSpeed);
    printf( ((localCopy->mRpm > 0.8*localCopy->mMaxRPM)? "SHIFT UP" : "" ));
    printf( (((localCopy->mRpm < 0.25*localCopy->mMaxRPM) && localCopy->mSpeed < 5 && localCopy->mGear > 1 )? "SHIFT DOWN" : "" ));
    if (localCopy->mRpm > 0.8*localCopy->mMaxRPM) {
      // Press Shift up
      ip.ki.wScan = DIKEYBOARD_P;
  		SendInput(1, &ip, sizeof(INPUT));
      Sleep(20);
      // Release the key
      ip.ki.dwFlags = KEYEVENTF_KEYUP; // KEYEVENTF_KEYUP for key release
      SendInput(1, &ip, sizeof(INPUT));

    } else if ((localCopy->mRpm < 0.25*localCopy->mMaxRPM) && localCopy->mSpeed < 5 && localCopy->mGear > 1 ) {
      // Press Shift down
      ip.ki.wScan = DIKEYBOARD_O;
      SendInput(1, &ip, sizeof(INPUT));
      Sleep(20);
      // Release the key
      ip.ki.dwFlags = KEYEVENTF_KEYUP; // KEYEVENTF_KEYUP for key release
      SendInput(1, &ip, sizeof(INPUT));
    }


    system("cls");

    if ( _kbhit() && _getch() == 27 ) // check for escape
    {
      break;
    }
  }
  //------------------------------------------------------------------------------

  // Cleanup
  UnmapViewOfFile( sharedData );
  CloseHandle( fileHandle );
  delete localCopy;

  return 0;
}
