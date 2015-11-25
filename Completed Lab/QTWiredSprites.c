//////////////	File:		QTWiredSprites.c////	Contains:	QuickTime wired sprites support for QuickTime movies.////	Written by:	Sean Allen//	Revised by:	Chris Flick and Tim Monroe//				Based (heavily!) on the existing MakeActionSpriteMovie.c code written by Sean Allen.////	Copyright:	� 1997-1998 by Apple Computer, Inc., all rights reserved.////	Change History (most recent first):////	   <5>	 	03/20/00	rtm		made changes to get things running under CarbonLib//	   <4>	 	07/30/99	rtm		added QTWired_AddCursorChangeOnMouseOver to illustrate new//									sprite-as-button behaviors added in QT4; added this action to penguin one//	   <3>	 	09/30/98	rtm		tweaked call to AddMovieResource to create single-fork movies//	   <2>	 	03/26/98	rtm		made fixes for Windows compiles//	   <1>	 	03/25/98	rtm		first file; integrated existing code with shell framework//	   ////	This sample code creates a wired sprite movie containing one sprite track. The sprite track contains//	six sprites: two penguins and four buttons.//	//	The four buttons are initially invisible. When the mouse enters (or "rolls over") a button, it appears.//	When the mouse is clicked inside a button, its image changes to its "pressed" image. When the mouse//	is released, its image changes back to its "unpressed" image. If the mouse is released inside the button, //	an action is triggered. The buttons perform the actions of go to beginning of movie, step backward,//	step forward, and go to end of movie.//	//	The first penguin shows all of the buttons when the mouse enters it, and hides them when the mouse exits.//	The first penguin is the only sprite that has properties that are overriden by the override sprite samples.//	These samples override its matrix (in order to move it) and its image index (in order to make it "waddle").//	//	When the mouse is clicked on the second penguin, it changes its image index to it's "eyes closed" image.//	When the mouse is released, it changes back to its normal image. This makes it appear to blink when clicked on.//	When the mouse is released over the penguin, several actions are triggered. Both penguins' graphics states are //	toggled between copyMode and blendMode, and the movie's rate is toggled between zero and one.//	//	The second penguin moves once per second. This occurs whether the movie's rate is currently zero or one,//	because it is being triggered by a gated idle event. When the penguin receives the idle event, it changes//	its matrix using an action which uses min, max, delta, and wraparound options.////	The movie's looping mode is set to palindrome by a frame-loaded action.////	So, our general strategy is as follows (though perhaps not in the order listed):////		(1) Create a new movie file with a single sprite track.//		(2) Assign the "no controller" movie controller to the movie.//		(3) Set the sprite track's background color, idle event frequency, and hasActions properties.//		(4) Convert our PICT resources to animation codec images with transparency.//		(5) Create a key frame sample containing six sprites and all of their shared images.//		(6) Assign the sprites their initial property values.//		(7)	Create a frameLoaded event for the key frame.//		(8)	Create some override samples that override the matrix and image index properties of//			the first penguin sprite.////	NOTES://		//	*** (1) ***//	There are event types other that mouse related events (for instance, Idle and FrameLoaded).//	Idle events are independent of the movie's rate, and they can be gated so they are send at most//	every n ticks. In our sample movie, the second penguin moves when the movie's rate is zero,//	and moves only once per second because of the value of the sprite track's idleEventFrequencey property.//		//	*** (2) ***//	Multiple actions may be executed in response to a single event. In our sample movie, rolling over//	the first penguin shows and hides four different buttons.//		//	*** (3) ***//	Actions may target any sprite or track in the movie. In our sample movie, clicking on one penguin//	changes the graphics mode of the other.//		//	*** (4) ***//	Conditional and looping control structures are supported. In our sample movie, the second penguin//	uses the "case statement" action.//		//	*** (5) ***//	Sprite track variables that have not been set have a default value of zero. (The second penguin's//	conditional code relies on this.)//		//	*** (6) ***//	Wired sprites were previously known as "action sprites". Don't let the names of some of the utility//	functions confuse you. We'll try to update the source code as time permits.//		//	*** (7) ***//	Penguins don't fly, but I hear they totally shred halfpipes on snowboards.////////////// header files#include "QTWiredSprites.h"////////////// QTWired_CreateWiredSpritesMovie// Create a QuickTime movie containing a wired sprites track.////////////OSErr QTWired_CreateWiredSpritesMovie (void){	short					myResRefNum = 0;	short					myResID = movieInDataForkResID;	Movie					myMovie = NULL;	Track					myTrack;	Media					myMedia;	FSSpec					myFile;	Boolean					myIsSelected = false;	Boolean					myIsReplacing = false;		StringPtr 				myPrompt = QTUtils_ConvertCToPascalString(kWiredSavePrompt);	StringPtr 				myFileName = QTUtils_ConvertCToPascalString(kWiredSaveFileName);	QTAtomContainer			mySample = NULL;	QTAtomContainer			myActions = NULL;	QTAtomContainer			myBeginButton, myPrevButton, myNextButton, myEndButton;	QTAtomContainer			myPenguinOne, myPenguinTwo, myPenguinOneOverride;	QTAtomContainer			myBeginActionButton, myPrevActionButton, myNextActionButton, myEndActionButton;	QTAtomContainer			myPenguinOneAction, myPenguinTwoAction;	RGBColor				myKeyColor;	Point					myLocation;	short					isVisible, myLayer, myIndex, myID, i, myDelta;	Boolean					hasActions;	long					myFlags = createMovieFileDeleteCurFile | createMovieFileDontCreateResFile;	OSType					myType = FOUR_CHAR_CODE('none');	UInt32					myFrequency;	QTAtom					myEventAtom;	long					myLoopingFlags;	ModifierTrackGraphicsModeRecord		myGraphicsMode;	OSErr					myErr = noErr;	//////////	//	// create a new movie file and set its controller type	//	//////////	// ask the user for the name of the new movie file	QTFrame_PutFile(myPrompt, myFileName, &myFile, &myIsSelected, &myIsReplacing);	if (!myIsSelected)		goto bail;	// create a movie file for the destination movie	myErr = CreateMovieFile(&myFile,				/* fsspec for the movie to be created */							FOUR_CHAR_CODE('TVOD'),	/* creator value for the new file */							smSystemScript,			/* script in which the movie file should be created */							myFlags,				/* movie creation flags */							&myResRefNum,			/* file reference for movie file return here */							&myMovie);				/* new movie identifier return here */	if (myErr != noErr)		goto bail;		// select the "no controller" movie controller option	myType = EndianU32_NtoB(myType);	SetUserDataItem(GetMovieUserData(myMovie),					&myType,					sizeof(myType),					kUserDataMovieControllerType,					1);		//////////	//	// create the sprite track and media	//	//////////		myTrack = NewMovieTrack(myMovie,							((long)kSpriteTrackWidth << 16),	/* display width in pixels */							((long)kSpriteTrackHeight << 16),	/* display height in pixels */							kNoVolume);							/* volume setting of the track */	myMedia = NewTrackMedia(myTrack,							SpriteMediaType,		/* type of media to create */							kSpriteMediaTimeScale,	/* media time scale */							NULL,					/* data ref, pass nil to use the file the movie was created in */							0);						/* data ref type, pass nil to use the file the movie was created in */	//////////	//	// create a key frame sample containing six sprites and all of their shared images	//	//////////	// create a new, empty key frame sample	myErr = QTNewAtomContainer(&mySample);	if (myErr != noErr)		goto bail;	myKeyColor.red = 0xffff;						// white	myKeyColor.green = 0xffff;	myKeyColor.blue = 0xffff;	// add images to the key frame sample	AddPICTImageToKeyFrameSample(mySample,						/* the key frame sample */								kGoToBeginningButtonUp,			/* pict resource id */								&myKeyColor,					/* RGBColor that should be used as the transparency color */								kGoToBeginningButtonUpIndex,	/* atom id for the kSpriteImageAtomType atom */								NULL,							/* optional sprite registration point */								NULL);							/* optional sprite image name */	AddPICTImageToKeyFrameSample(mySample, kGoToBeginningButtonDown, &myKeyColor, kGoToBeginningButtonDownIndex, NULL, NULL);	AddPICTImageToKeyFrameSample(mySample, kGoToEndButtonUp, &myKeyColor, kGoToEndButtonUpIndex, NULL, NULL);	AddPICTImageToKeyFrameSample(mySample, kGoToEndButtonDown, &myKeyColor, kGoToEndButtonDownIndex, NULL, NULL);	AddPICTImageToKeyFrameSample(mySample, kGoToPrevButtonUp, &myKeyColor, kGoToPrevButtonUpIndex, NULL, NULL);	AddPICTImageToKeyFrameSample(mySample, kGoToPrevButtonDown, &myKeyColor, kGoToPrevButtonDownIndex, NULL, NULL);	AddPICTImageToKeyFrameSample(mySample, kGoToNextButtonUp, &myKeyColor, kGoToNextButtonUpIndex, NULL, NULL);	AddPICTImageToKeyFrameSample(mySample, kGoToNextButtonDown, &myKeyColor, kGoToNextButtonDownIndex, NULL, NULL);	AddPICTImageToKeyFrameSample(mySample, kPenguinForward, &myKeyColor, kPenguinForwardIndex, NULL, NULL);	AddPICTImageToKeyFrameSample(mySample, kPenguinLeft, &myKeyColor, kPenguinLeftIndex, NULL, NULL);	AddPICTImageToKeyFrameSample(mySample, kPenguinRight, &myKeyColor, kPenguinRightIndex, NULL, NULL);	AddPICTImageToKeyFrameSample(mySample, kPenguinClosed, &myKeyColor, kPenguinClosedIndex, NULL, NULL);	for (myIndex = kPenguinDownRightCycleStartIndex, myID = kWalkDownRightCycleStart; myIndex <= kPenguinDownRightCycleEndIndex; myIndex++, myID++)		AddPICTImageToKeyFrameSample(mySample, myID, &myKeyColor, myIndex, NULL, NULL);	/* assign group IDs to the images - you must assign group IDs to your sprite sample		if you want a sprite to display images with non-equivalent image descriptions 		(i.e., images with different dimensions) */	AssignImageGroupIDsToKeyFrame(mySample);		//////////	//	// add samples to the sprite track's media	//	//////////		BeginMediaEdits(myMedia);	// go to beginning button with no actions	myErr = QTNewAtomContainer(&myBeginButton);	if (myErr != noErr)		goto bail;	myLocation.h	= (1 * kSpriteTrackWidth / 8) - (kStartEndButtonWidth / 2);	myLocation.v	= (4 * kSpriteTrackHeight / 5) - (kStartEndButtonHeight / 2);	isVisible		= false;	myLayer			= 1;	myIndex			= kGoToBeginningButtonUpIndex;	myErr = SetSpriteData(myBeginButton,						&myLocation,	/* sprite location */						&isVisible,		/* sprite visible property */						&myLayer,		/* sprite layer (smaller #'s are further forward) */						&myIndex,		/* sprite index property */						NULL,						NULL,						myActions);	if (myErr != noErr)		goto bail;	// go to previous button with no actions	myErr = QTNewAtomContainer(&myPrevButton);	if (myErr != noErr)		goto bail;	myLocation.h 	= (3 * kSpriteTrackWidth / 8) - (kNextPrevButtonWidth / 2);	myLocation.v	= (4 * kSpriteTrackHeight / 5) - (kStartEndButtonHeight / 2);	isVisible		= false;	myLayer			= 1;	myIndex			= kGoToPrevButtonUpIndex;	myErr = SetSpriteData(myPrevButton, &myLocation, &isVisible, &myLayer, &myIndex, NULL, NULL, myActions);	if (myErr != noErr)		goto bail;	// go to next button with no actions	myErr = QTNewAtomContainer(&myNextButton);	if (myErr != noErr)		goto bail;	myLocation.h 	= (5 * kSpriteTrackWidth / 8) - (kNextPrevButtonWidth / 2);	myLocation.v	= (4 * kSpriteTrackHeight / 5) - (kStartEndButtonHeight / 2);	isVisible		= false;	myLayer			= 1;	myIndex			= kGoToNextButtonUpIndex;	myErr = SetSpriteData(myNextButton, &myLocation, &isVisible, &myLayer, &myIndex, NULL, NULL, myActions);	if (myErr != noErr)		goto bail;	// go to end button with no actions	myErr = QTNewAtomContainer(&myEndButton);	if (myErr != noErr)		goto bail;	myLocation.h 	= (7 * kSpriteTrackWidth / 8) - (kStartEndButtonWidth / 2);	myLocation.v	= (4 * kSpriteTrackHeight / 5) - (kStartEndButtonHeight / 2);	isVisible		= false;	myLayer			= 1;	myIndex			= kGoToEndButtonUpIndex;	myErr = SetSpriteData(myEndButton, &myLocation, &isVisible, &myLayer, &myIndex, NULL, NULL, myActions);	if (myErr != noErr)		goto bail;	// first penguin sprite with no actions	myErr = QTNewAtomContainer(&myPenguinOne);	if (myErr != noErr)		goto bail;	myLocation.h 	= (3 * kSpriteTrackWidth / 8) - (kPenguinWidth / 2);	myLocation.v 	= (kSpriteTrackHeight / 4) - (kPenguinHeight / 2);	isVisible		= true;	myLayer			= 2;	myIndex			= kPenguinDownRightCycleStartIndex;	myGraphicsMode.graphicsMode = blend;	myGraphicsMode.opColor.red = myGraphicsMode.opColor.green = myGraphicsMode.opColor.blue = 0x8FFF;	// grey	myErr = SetSpriteData(myPenguinOne, &myLocation, &isVisible, &myLayer, &myIndex, &myGraphicsMode, NULL, myActions);	if (myErr != noErr)		goto bail;			// second penguin sprite with no actions	myErr = QTNewAtomContainer(&myPenguinTwo);	if (myErr != noErr)		goto bail;	myLocation.h 	= (5 * kSpriteTrackWidth / 8) - (kPenguinWidth / 2);	myLocation.v 	= (kSpriteTrackHeight / 4) - (kPenguinHeight / 2);	isVisible		= true;	myLayer			= 3;	myIndex			= kPenguinForwardIndex;	myErr = SetSpriteData(myPenguinTwo, &myLocation, &isVisible, &myLayer, &myIndex, NULL, NULL, myActions);	if (myErr != noErr)		goto bail;	//////////	//	// add actions to the six sprites	//	//////////	// add go to beginning button	myErr = QTCopyAtom(myBeginButton, kParentAtomIsContainer, &myBeginActionButton);	if (myErr != noErr)		goto bail;	AddSpriteSetImageIndexAction(myBeginActionButton, kParentAtomIsContainer, kQTEventMouseClick, 0, NULL, 0, 0, NULL, kGoToBeginningButtonDownIndex, NULL);	AddSpriteSetImageIndexAction(myBeginActionButton, kParentAtomIsContainer, kQTEventMouseClickEnd, 0, NULL, 0, 0, NULL, kGoToBeginningButtonUpIndex, NULL);	AddMovieGoToBeginningAction(myBeginActionButton, kParentAtomIsContainer, kQTEventMouseClickEndTriggerButton);	AddSpriteSetVisibleAction(myBeginActionButton, kParentAtomIsContainer, kQTEventMouseEnter, 0, NULL, 0, 0, NULL, true, NULL);	AddSpriteSetVisibleAction(myBeginActionButton, kParentAtomIsContainer, kQTEventMouseExit, 0, NULL, 0, 0, NULL, false, NULL);	AddSpriteToSample(mySample, myBeginActionButton, kGoToBeginningSpriteID);		QTDisposeAtomContainer(myBeginActionButton);	// add go to prev button	myErr = QTCopyAtom(myPrevButton, kParentAtomIsContainer, &myPrevActionButton);	if (myErr != noErr)		goto bail;	AddSpriteSetImageIndexAction(myPrevActionButton, kParentAtomIsContainer, kQTEventMouseClick, 0, NULL, 0, 0, NULL, kGoToPrevButtonDownIndex, NULL);	AddSpriteSetImageIndexAction(myPrevActionButton, kParentAtomIsContainer, kQTEventMouseClickEnd, 0, NULL, 0, 0, NULL, kGoToPrevButtonUpIndex, NULL);	AddMovieStepBackwardAction(myPrevActionButton, kParentAtomIsContainer, kQTEventMouseClickEndTriggerButton);	AddSpriteSetVisibleAction(myBeginActionButton, kParentAtomIsContainer, kQTEventMouseEnter, 0, NULL, 0, 0, NULL, true, NULL);	AddSpriteSetVisibleAction(myBeginActionButton, kParentAtomIsContainer, kQTEventMouseExit, 0, NULL, 0, 0, NULL, false, NULL);	AddSpriteToSample(mySample, myPrevActionButton, kGoToPrevSpriteID);	QTDisposeAtomContainer(myPrevActionButton);	// add go to next button	myErr = QTCopyAtom(myNextButton, kParentAtomIsContainer, &myNextActionButton);	if (myErr != noErr)		goto bail;	AddSpriteSetImageIndexAction(myNextActionButton, kParentAtomIsContainer, kQTEventMouseClick, 0, NULL, 0, 0, NULL, kGoToNextButtonDownIndex, NULL);	AddSpriteSetImageIndexAction(myNextActionButton, kParentAtomIsContainer, kQTEventMouseClickEnd, 0, NULL, 0, 0, NULL, kGoToNextButtonUpIndex, NULL);	AddMovieStepForwardAction(myNextActionButton, kParentAtomIsContainer, kQTEventMouseClickEndTriggerButton);	AddSpriteSetVisibleAction(myBeginActionButton, kParentAtomIsContainer, kQTEventMouseEnter, 0, NULL, 0, 0, NULL, true, NULL);	AddSpriteSetVisibleAction(myBeginActionButton, kParentAtomIsContainer, kQTEventMouseExit, 0, NULL, 0, 0, NULL, false, NULL);	AddSpriteToSample(mySample, myNextActionButton, kGoToNextSpriteID);	QTDisposeAtomContainer(myNextActionButton);	// add go to end button	myErr = QTCopyAtom(myEndButton, kParentAtomIsContainer, &myEndActionButton);	if (myErr != noErr)		goto bail;	AddSpriteSetImageIndexAction(myEndActionButton, kParentAtomIsContainer, kQTEventMouseClick, 0, NULL, 0, 0, NULL, kGoToEndButtonDownIndex, NULL);	AddSpriteSetImageIndexAction(myEndActionButton, kParentAtomIsContainer, kQTEventMouseClickEnd, 0, NULL, 0, 0, NULL, kGoToEndButtonUpIndex, NULL);	AddMovieGoToEndAction(myEndActionButton, kParentAtomIsContainer, kQTEventMouseClickEndTriggerButton);	AddSpriteSetVisibleAction(myBeginActionButton, kParentAtomIsContainer, kQTEventMouseEnter, 0, NULL, 0, 0, NULL, true, NULL);	AddSpriteSetVisibleAction(myBeginActionButton, kParentAtomIsContainer, kQTEventMouseExit, 0, NULL, 0, 0, NULL, false, NULL);	AddSpriteToSample(mySample, myEndActionButton, kGoToEndSpriteID);	QTDisposeAtomContainer(myEndActionButton);	// add penguin one	myErr = QTCopyAtom(myPenguinOne, kParentAtomIsContainer, &myPenguinOneAction);	if (myErr != noErr)		goto bail;	// show the buttons on mouse enter and hide them on mouse exit	AddSpriteSetVisibleAction(myPenguinOneAction, kParentAtomIsContainer, kQTEventMouseEnter, 0, NULL, 0, kTargetSpriteID, (void *)kGoToBeginningSpriteID, true, NULL);	AddSpriteSetVisibleAction(myPenguinOneAction, kParentAtomIsContainer, kQTEventMouseExit, 0, NULL, 0, kTargetSpriteID, (void *)kGoToBeginningSpriteID, false, NULL);	AddSpriteSetVisibleAction(myPenguinOneAction, kParentAtomIsContainer, kQTEventMouseEnter, 0, NULL, 0, kTargetSpriteID, (void *)kGoToPrevSpriteID, true, NULL);	AddSpriteSetVisibleAction(myPenguinOneAction, kParentAtomIsContainer, kQTEventMouseExit, 0, NULL, 0, kTargetSpriteID, (void *)kGoToPrevSpriteID, false, NULL);	AddSpriteSetVisibleAction(myPenguinOneAction, kParentAtomIsContainer, kQTEventMouseEnter, 0, NULL, 0, kTargetSpriteID, (void *)kGoToNextSpriteID, true, NULL);	AddSpriteSetVisibleAction(myPenguinOneAction, kParentAtomIsContainer, kQTEventMouseExit, 0, NULL, 0, kTargetSpriteID, (void *)kGoToNextSpriteID, false, NULL);	AddSpriteSetVisibleAction(myPenguinOneAction, kParentAtomIsContainer, kQTEventMouseEnter, 0, NULL, 0, kTargetSpriteID, (void *)kGoToEndSpriteID, true, NULL);	AddSpriteSetVisibleAction(myPenguinOneAction, kParentAtomIsContainer, kQTEventMouseExit, 0, NULL, 0, kTargetSpriteID, (void *)kGoToEndSpriteID, false, NULL);	AddSpriteToSample(mySample, myPenguinOneAction, kPenguinOneSpriteID);	QTWired_AddCursorChangeOnMouseOver(mySample, kPenguinOneSpriteID);	QTDisposeAtomContainer(myPenguinOneAction);	// add penguin two	myErr = QTCopyAtom(myPenguinTwo, kParentAtomIsContainer, &myPenguinTwoAction);	if (myErr != noErr)		goto bail;	// blink when clicked on	AddSpriteSetImageIndexAction(myPenguinTwoAction, kParentAtomIsContainer, kQTEventMouseClick, 0, NULL, 0, 0, NULL, kPenguinClosedIndex, NULL);	AddSpriteSetImageIndexAction(myPenguinTwoAction, kParentAtomIsContainer, kQTEventMouseClickEnd, 0, NULL, 0, 0, NULL, kPenguinForwardIndex, NULL);	AddQTEventAtom(myPenguinTwoAction, kParentAtomIsContainer, kQTEventMouseClickEndTriggerButton, &myEventAtom);	// toggle the movie rate and both of the birds' graphics modes	QTWired_AddPenguinTwoConditionalActions(myPenguinTwoAction, myEventAtom);	QTWired_AddWraparoundMatrixOnIdle(myPenguinTwoAction);	AddSpriteToSample(mySample, myPenguinTwoAction, kPenguinTwoSpriteID);	QTDisposeAtomContainer(myPenguinTwoAction);		// add an action for when the key frame is loaded, to set the movie's looping mode to palindrome;	// note that this will actually be triggered every time the key frame is reloaded,	// so if the operation was expensive we could use a conditional to test if we've already done it	myLoopingFlags = loopTimeBase | palindromeLoopTimeBase;	AddMovieSetLoopingFlagsAction(mySample, kParentAtomIsContainer, kQTEventFrameLoaded, myLoopingFlags);	// add the key frame sample to the sprite track media	//	// to add the sample data in a compressed form, you would use a QuickTime DataCodec to perform the	// compression; replace the call to the utility AddSpriteSampleToMedia with a call to the utility	// AddCompressedSpriteSampleToMedia to do this		AddSpriteSampleToMedia(myMedia, mySample, kSpriteMediaFrameDuration, true, NULL);		//////////	//	// add a few override samples to move penguin one and change its image index	//	//////////	// original penguin one location	myLocation.h 	= (3 * kSpriteTrackWidth / 8) - (kPenguinWidth / 2);	myLocation.v 	= (kSpriteTrackHeight / 4) - (kPenguinHeight / 2);	myDelta = (kSpriteTrackHeight / 2) / kNumOverrideSamples;	myIndex = kPenguinDownRightCycleStartIndex;		for (i = 1; i <= kNumOverrideSamples; i++) {		QTRemoveChildren(mySample, kParentAtomIsContainer);		QTNewAtomContainer(&myPenguinOneOverride);		myLocation.h += myDelta;		myLocation.v += myDelta;		myIndex++;		if (myIndex > kPenguinDownRightCycleEndIndex)			myIndex = kPenguinDownRightCycleStartIndex;					SetSpriteData(myPenguinOneOverride, &myLocation, NULL, NULL, &myIndex, NULL, NULL, NULL);		AddSpriteToSample(mySample, myPenguinOneOverride, kPenguinOneSpriteID);		AddSpriteSampleToMedia(myMedia, mySample, kSpriteMediaFrameDuration, false, NULL);			QTDisposeAtomContainer(myPenguinOneOverride);	}	EndMediaEdits(myMedia);	// add the media to the track	InsertMediaIntoTrack(myTrack, 0, 0, GetMediaDuration(myMedia), fixed1);		//////////	//	// set the sprite track properties	//	//////////	{		QTAtomContainer		myTrackProperties;		RGBColor			myBackgroundColor;				// add a background color to the sprite track		myBackgroundColor.red = EndianU16_NtoB(0x8000);		myBackgroundColor.green = EndianU16_NtoB(0);		myBackgroundColor.blue = EndianU16_NtoB(0xffff);				QTNewAtomContainer(&myTrackProperties);		QTInsertChild(myTrackProperties, 0, kSpriteTrackPropertyBackgroundColor, 1, 1, sizeof(RGBColor), &myBackgroundColor, NULL);		// tell the movie controller that this sprite track has actions, Jackson		hasActions = true;		QTInsertChild(myTrackProperties, 0, kSpriteTrackPropertyHasActions, 1, 1, sizeof(hasActions), &hasActions, NULL);			// tell the sprite track to generate QTIdleEvents		myFrequency = EndianU32_NtoB(60);		QTInsertChild(myTrackProperties, 0, kSpriteTrackPropertyQTIdleEventsFrequency, 1, 1, sizeof(myFrequency), &myFrequency, NULL);		myErr = SetMediaPropertyAtom(myMedia, myTrackProperties);		if (myErr != noErr)			goto bail;		QTDisposeAtomContainer(myTrackProperties);	}	//////////	//	// finish up	//	//////////		// add the movie resource to the movie file	myErr = AddMovieResource(myMovie, myResRefNum, &myResID, myFile.name);	bail:	free(myPrompt);	free(myFileName);	if (mySample != NULL)		QTDisposeAtomContainer(mySample);	if (myBeginButton != NULL)		QTDisposeAtomContainer(myBeginButton);					if (myPrevButton != NULL)		QTDisposeAtomContainer(myPrevButton);					if (myNextButton != NULL)		QTDisposeAtomContainer(myNextButton);					if (myEndButton != NULL)		QTDisposeAtomContainer(myEndButton);					if (myResRefNum != 0)		CloseMovieFile(myResRefNum);	if (myMovie != NULL)		DisposeMovie(myMovie);			return(myErr);}////////////// QTWired_AddPenguinTwoConditionalActions// Add actions to the second penguin that transform him (her?) into a two state button// that plays or pauses the movie.//// We are relying on the fact that a "GetVariable" for a variable ID which has never been set// will return zero. If we needed a different default value, we could initialize it using the// frameLoaded event.//// A higher-level description of the logic is:// // 	On MouseUpInside// 	   If (GetVariable(DefaultTrack, 1) = 0)// 	      SetMovieRate(1)// 	      SetSpriteGraphicsMode(DefaultSprite, { blend, grey } )// 	      SetSpriteGraphicsMode(GetSpriteByID(DefaultTrack, 5), { ditherCopy, white } )// 	      SetVariable(DefaultTrack, 1, 1)// 	   ElseIf (GetVariable(DefaultTrack, 1) = 1)// 	      SetMovieRate(0)// 	      SetSpriteGraphicsMode(DefaultSprite, { ditherCopy, white })// 	      SetSpriteGraphicsMode(GetSpriteByID(DefaultTrack, 5), { blend, grey })// 	      SetVariable(DefaultTrack, 1, 0)// 	   Endif// 	End// //////////OSErr QTWired_AddPenguinTwoConditionalActions (QTAtomContainer theContainer, QTAtom theEventAtom){	QTAtom								myNewActionAtom, myNewParamAtom, myConditionalAtom;	QTAtom								myExpressionAtom, myOperatorAtom, myActionListAtom;	short								myParamIndex, myConditionIndex, myOperandIndex;	float								myConstantValue;	QTAtomID							myVariableID;	ModifierTrackGraphicsModeRecord		myBlendMode, myCopyMode;		myBlendMode.graphicsMode = blend;	myBlendMode.opColor.red = myBlendMode.opColor.green = myBlendMode.opColor.blue = 0x8fff;	// grey	myCopyMode.graphicsMode = ditherCopy;	myCopyMode.opColor.red = myCopyMode.opColor.green = myCopyMode.opColor.blue = 0xffff;		// white	AddActionAtom(theContainer, theEventAtom, kActionCase, &myNewActionAtom);		myParamIndex = 1;	AddActionParameterAtom(theContainer, myNewActionAtom, myParamIndex, 0, NULL, &myNewParamAtom);	// first condition	myConditionIndex = 1;	AddConditionalAtom(theContainer, myNewParamAtom, myConditionIndex, &myConditionalAtom);	AddExpressionContainerAtomType(theContainer, myConditionalAtom, &myExpressionAtom);	AddOperatorAtom(theContainer, myExpressionAtom, kOperatorEqualTo, &myOperatorAtom);	myOperandIndex = 1;	myConstantValue = kButtonStateOne;	AddOperandAtom(theContainer, myOperatorAtom, kOperandConstant, myOperandIndex, NULL, myConstantValue);	myOperandIndex = 2;	myVariableID = kPenguinStateVariableID;	AddVariableOperandAtom(theContainer, myOperatorAtom, myOperandIndex, 0, NULL, 0, myVariableID);	AddActionListAtom(theContainer, myConditionalAtom, &myActionListAtom);	AddMovieSetRateAction(theContainer, myActionListAtom, 0, Long2Fix(1));	AddSpriteSetGraphicsModeAction(theContainer, myActionListAtom, 0, 0, NULL, 0, 0, NULL, &myBlendMode, NULL);	AddSpriteSetGraphicsModeAction(theContainer, myActionListAtom, 0, 0, NULL, 0, kTargetSpriteID, (void *)kPenguinOneSpriteID, &myCopyMode, NULL);	AddSpriteTrackSetVariableAction(theContainer, myActionListAtom, 0, kPenguinStateVariableID, kButtonStateTwo, 0, NULL, 0);									   	// second condition	myConditionIndex = 2;	AddConditionalAtom(theContainer, myNewParamAtom, myConditionIndex, &myConditionalAtom);	AddExpressionContainerAtomType(theContainer, myConditionalAtom, &myExpressionAtom);	AddOperatorAtom(theContainer, myExpressionAtom, kOperatorEqualTo, &myOperatorAtom);	myOperandIndex = 1;	myConstantValue = kButtonStateTwo;	AddOperandAtom(theContainer, myOperatorAtom, kOperandConstant, myOperandIndex, NULL, myConstantValue);	myOperandIndex = 2;	myVariableID = kPenguinStateVariableID;	AddVariableOperandAtom(theContainer, myOperatorAtom, myOperandIndex, 0, NULL, 0, myVariableID);	AddActionListAtom(theContainer, myConditionalAtom, &myActionListAtom);	AddMovieSetRateAction(theContainer, myActionListAtom, 0, Long2Fix(0));	AddSpriteSetGraphicsModeAction(theContainer, myActionListAtom, 0, 0, NULL, 0, 0, NULL, &myCopyMode, NULL);	AddSpriteSetGraphicsModeAction(theContainer, myActionListAtom, 0, 0, NULL, 0, kTargetSpriteID, (void *)kPenguinOneSpriteID, &myBlendMode, NULL);	AddSpriteTrackSetVariableAction(theContainer, myActionListAtom, 0, kPenguinStateVariableID, kButtonStateOne, 0, NULL, 0);	return(noErr);}////////////// QTWired_AddWraparoundMatrixOnIdle// Add beginning, end, and change matrices to the specified atom container.////////////OSErr QTWired_AddWraparoundMatrixOnIdle (QTAtomContainer theContainer){	MatrixRecord 	myMinMatrix, myMaxMatrix, myDeltaMatrix;	long			myFlags = kActionFlagActionIsDelta | kActionFlagParameterWrapsAround;	QTAtom			myActionAtom;	OSErr			myErr = noErr;		myMinMatrix.matrix[0][0] = myMinMatrix.matrix[0][1] = myMinMatrix.matrix[0][2] = EndianS32_NtoB(0xffffffff);	myMinMatrix.matrix[1][0] = myMinMatrix.matrix[1][1] = myMinMatrix.matrix[1][2] = EndianS32_NtoB(0xffffffff);	myMinMatrix.matrix[2][0] = myMinMatrix.matrix[2][1] = myMinMatrix.matrix[2][2] = EndianS32_NtoB(0xffffffff);	myMaxMatrix.matrix[0][0] = myMaxMatrix.matrix[0][1] = myMaxMatrix.matrix[0][2] = EndianS32_NtoB(0x7fffffff);	myMaxMatrix.matrix[1][0] = myMaxMatrix.matrix[1][1] = myMaxMatrix.matrix[1][2] = EndianS32_NtoB(0x7fffffff);	myMaxMatrix.matrix[2][0] = myMaxMatrix.matrix[2][1] = myMaxMatrix.matrix[2][2] = EndianS32_NtoB(0x7fffffff);		myMinMatrix.matrix[2][1] = EndianS32_NtoB(Long2Fix((1 * kSpriteTrackHeight / 4) - (kPenguinHeight / 2)));	myMaxMatrix.matrix[2][1] = EndianS32_NtoB(Long2Fix((3 * kSpriteTrackHeight / 4) - (kPenguinHeight / 2)));	SetIdentityMatrix(&myDeltaMatrix);	myDeltaMatrix.matrix[2][1] = Long2Fix(1);		// change location	myErr = AddSpriteSetMatrixAction(theContainer, kParentAtomIsContainer, kQTEventIdle, 0, NULL, 0, 0, NULL, &myDeltaMatrix, &myActionAtom);	if (myErr != noErr)		goto bail;	myErr = AddActionParameterOptions(theContainer, myActionAtom, 1, myFlags, sizeof(myMinMatrix), &myMinMatrix, sizeof(myMaxMatrix), &myMaxMatrix);bail:	return(myErr);}////////////// QTWired_AddCursorChangeOnMouseOver// Add a cursor-change-on-mouse-over action to the sprite having the specified ID// in the specified atom container.//// Here we use the new "sprite behaviors atom" introduced in QuickTime 4.0, which// simplifies adding button-like capabilities to sprites.////////////OSErr QTWired_AddCursorChangeOnMouseOver (QTAtomContainer theContainer, QTAtomID theID){	QTAtom								mySpriteAtom = 0;	QTAtom								myBehaviorAtom = 0;	QTSpriteButtonBehaviorStruct		myBehaviorRec;	OSErr								myErr = noErr;		// find the sprite atom with the specified ID in the specified container	mySpriteAtom = QTFindChildByID(theContainer, kParentAtomIsContainer, kSpriteAtomType, theID, NULL);	if (mySpriteAtom == 0) {		// if there is none, insert a new sprite atom into the specified container		myErr = QTInsertChild(theContainer, kParentAtomIsContainer, kSpriteAtomType, theID, 0, 0, NULL, &mySpriteAtom);		if (myErr != noErr)			goto bail;	}		// insert a new sprite behaviors atom into the sprite atom	myErr = QTInsertChild(theContainer, mySpriteAtom, kSpriteBehaviorsAtomType, 1, 1, 0, NULL, &myBehaviorAtom);	if (myErr != noErr)		goto bail;	//////////	//	// insert three atoms into the sprite behaviors atom; these three atoms specify what to do on each	// of the four defined state transitions for the (1) sprite image, (2) cursor, and (3) status string	//	//////////		// set the sprite image behavior; -1 means: no change associated with this state transition	myBehaviorRec.notOverNotPressedStateID = EndianS32_NtoB(-1);	myBehaviorRec.overNotPressedStateID = EndianS32_NtoB(-1);	myBehaviorRec.overPressedStateID = EndianS32_NtoB(-1);	myBehaviorRec.notOverPressedStateID = EndianS32_NtoB(-1);	myErr = QTInsertChild(theContainer, myBehaviorAtom, kSpriteImageBehaviorAtomType, 1, 1, sizeof(QTSpriteButtonBehaviorStruct), &myBehaviorRec, NULL);	if (myErr != noErr)		goto bail;		// set the sprite cursor behavior; -1 means: no change associated with this state transition	myBehaviorRec.notOverNotPressedStateID = EndianS32_NtoB(-1);	myBehaviorRec.overNotPressedStateID = EndianS32_NtoB(kQTCursorOpenHand);	myBehaviorRec.overPressedStateID = EndianS32_NtoB(-1);	myBehaviorRec.notOverPressedStateID = EndianS32_NtoB(-1);	myErr = QTInsertChild(theContainer, myBehaviorAtom, kSpriteCursorBehaviorAtomType, 1, 1, sizeof(QTSpriteButtonBehaviorStruct), &myBehaviorRec, NULL);	if (myErr != noErr)		goto bail;		// set the status string behavior; -1 means: no change associated with this state transition	myBehaviorRec.notOverNotPressedStateID = EndianS32_NtoB(-1);	myBehaviorRec.overNotPressedStateID = EndianS32_NtoB(-1);	myBehaviorRec.overPressedStateID = EndianS32_NtoB(-1);	myBehaviorRec.notOverPressedStateID = EndianS32_NtoB(-1);	myErr = QTInsertChild(theContainer, myBehaviorAtom, kSpriteStatusStringsBehaviorAtomType, 1, 1, sizeof(QTSpriteButtonBehaviorStruct), &myBehaviorRec, NULL);	if (myErr != noErr)		goto bail;			bail:	return(myErr);}