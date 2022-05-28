#include "GameAPI.h"
#include <list>

/************************************************************
	Config Variables (Set these to whatever you need. They are automatically read by the game.)
*************************************************************/
float TickRate = 1;
const int UndoHistoryLength = 5;

// Unique Mod IDS
//********************************
const int PaintBlock = 3022;
const int UndoBlock = 3023;
const int Marker1Block = 3024;
const int Marker2Block = 3025;
const int MaskBlock = 3026;
const int AirFilter = 3027;
const int ToggleWandBlock = 3028;
const int CopyBlock = 3029;
const int CutBlock = 3030;
const int PasteBlock = 3031;
const int Rotate90CWBlock = 3032;
const int RedoBlock = 3033;
const int Rotate90CCWBlock = 3034;
const int PaletteBlock = 3035;
const int ToggleExchangeBlock = 3036;

UniqueID ThisModUniqueIDs[] = { 
	PaintBlock, UndoBlock, Marker1Block, Marker2Block, MaskBlock, ToggleWandBlock, CopyBlock, CutBlock, 
	PasteBlock, Rotate90CWBlock, RedoBlock, Rotate90CCWBlock, PaletteBlock, ToggleExchangeBlock };

// Data Structs
//********************************
struct Block {
	BlockInfo blockInfo;
	CoordinateInBlocks location;

	Block(BlockInfo info, CoordinateInBlocks cords) {
		blockInfo = info;
		location = cords;
	}
};
struct PaintOperation {
	std::vector<Block> paintedBlocks;

	PaintOperation ExecutePaint() {
		std::vector<Block> reverseOperation;


		for (int i = 0; i <= paintedBlocks.size(); i++) {
			reverseOperation.push_back(
				Block(
				GetAndSetBlock(paintedBlocks[i].location, paintedBlocks[i].blockInfo),
				paintedBlocks[i].location
				)
			);
		}
		return PaintOperation(reverseOperation);
	}
};

// State Variables
//********************************
CoordinateInBlocks marker1Cord;
CoordinateInBlocks marker2Cord;
CoordinateInBlocks maskCord;
CoordinateInBlocks paintCord;

bool selectionWandEnabled = false;
bool exchangingWandEnabled = false;

std::list<PaintOperation> undoHistory;
std::list<PaintOperation> redoHistory;
std::vector<Block> clipboard;

int64_t clipboardWidth;
int64_t clipboardLength;

BlockInfo exchangeTarget(EBlockType::Air);

// Utility Methods
//********************************
CoordinateInBlocks GetSmallVector(CoordinateInBlocks cord1, CoordinateInBlocks cord2) {
	int64_t x = (cord1.X < cord2.X) ? cord1.X : cord2.X;
	int64_t y = (cord1.Y < cord2.Y) ? cord1.Y : cord2.Y;
	int16_t z = (cord1.Z < cord2.Z) ? cord1.Z : cord2.Z;

	return CoordinateInBlocks(x, y, z);
}
CoordinateInBlocks GetLargeVector(CoordinateInBlocks cord1, CoordinateInBlocks cord2) {
	int64_t x = (cord1.X > cord2.X) ? cord1.X : cord2.X;
	int64_t y = (cord1.Y > cord2.Y) ? cord1.Y : cord2.Y;
	int16_t z = (cord1.Z > cord2.Z) ? cord1.Z : cord2.Z;

	return CoordinateInBlocks(x, y, z);
}
CoordinateInBlocks GetBlockAbove(CoordinateInBlocks At) {
	return At + CoordinateInBlocks(0, 0, 1);
}


// Palette Methods
//********************************
bool TryGeneratePalette(CoordinateInBlocks At) {
	for (int z = 0; z <= 2; z++) {
		for (int x = 0; x <= 6; x++) {
			BlockInfo currentBlock = GetBlock(At + CoordinateInBlocks(x, 0, z));
			if (currentBlock.Type != EBlockType::Air) {
				SpawnHintText( At + CoordinateInBlocks(0,0,1), L"No space for palette here!", 1, 1);
				return false;
			}
		}
	}
	
	SetBlock(At + CoordinateInBlocks(1, 0, 0), BlockInfo(PaintBlock));
	SetBlock(At + CoordinateInBlocks(2, 0, 0), BlockInfo(MaskBlock));
	SetBlock(At + CoordinateInBlocks(3, 0, 0), BlockInfo(UndoBlock));
	SetBlock(At + CoordinateInBlocks(3, 0, 2), BlockInfo(RedoBlock));
	SetBlock(At + CoordinateInBlocks(4, 0, 0), BlockInfo(CopyBlock));
	SetBlock(At + CoordinateInBlocks(4, 0, 2), BlockInfo(CutBlock));
	SetBlock(At + CoordinateInBlocks(5, 0, 0), BlockInfo(Rotate90CWBlock));
	SetBlock(At + CoordinateInBlocks(5, 0, 2), BlockInfo(Rotate90CCWBlock));
	SetBlock(At + CoordinateInBlocks(6, 0, 0), BlockInfo(ToggleWandBlock));
	SetBlock(At + CoordinateInBlocks(6, 0, 2), BlockInfo(ToggleExchangeBlock));
	return true;
}

void RemovePalette(CoordinateInBlocks At) {
	SetBlock(At + CoordinateInBlocks(1, 0, 0), EBlockType::Air);
	SetBlock(At + CoordinateInBlocks(2, 0, 0), EBlockType::Air);
	SetBlock(At + CoordinateInBlocks(3, 0, 0), EBlockType::Air);
	SetBlock(At + CoordinateInBlocks(3, 0, 2), EBlockType::Air);
	SetBlock(At + CoordinateInBlocks(4, 0, 0), EBlockType::Air);
	SetBlock(At + CoordinateInBlocks(4, 0, 2), EBlockType::Air);
	SetBlock(At + CoordinateInBlocks(5, 0, 0), EBlockType::Air);
	SetBlock(At + CoordinateInBlocks(5, 0, 2), EBlockType::Air);
	SetBlock(At + CoordinateInBlocks(6, 0, 0), EBlockType::Air);
	SetBlock(At + CoordinateInBlocks(6, 0, 2), EBlockType::Air);
}

// Masking Methods
//********************************
std::vector<BlockInfo> GetMaskBlocks()
{
	std::vector<BlockInfo> maskBlocks;
	BlockInfo block = GetBlock(maskCord + CoordinateInBlocks(0, 0, 1));
	int i = 1;
	while (block.Type != EBlockType::Air) {
		maskBlocks.push_back(block);
		i++;
		block = GetBlock(maskCord + CoordinateInBlocks(0, 0, i));
	}
	return maskBlocks;
}
bool BlockIsMaskTarget(BlockInfo info, std::vector<BlockInfo> mask) {
	for (BlockInfo maskInfo : mask) {
		if (maskInfo.CustomBlockID == AirFilter && info.Type == EBlockType::Air)
			return true;
		if (info.Type == maskInfo.Type && info.CustomBlockID == maskInfo.CustomBlockID)
			return true;
	}
	return false;
}

// Undo Methods
//********************************
void AddRedoOperation(const PaintOperation& paintOp) {
	if (redoHistory.size() >= UndoHistoryLength) {
		redoHistory.pop_back();
	}
	redoHistory.push_front(paintOp);
}
void AddUndoOperation(const PaintOperation& paintOp) {
	if (undoHistory.size() >= UndoHistoryLength) {
		undoHistory.pop_back();
	}
	undoHistory.push_front(paintOp);
}

void UndoLastOperation() {
	if (undoHistory.empty()) return;

	PaintOperation paintOp = undoHistory.front();
	AddRedoOperation(paintOp.ExecutePaint());
	undoHistory.pop_front();
}

void RedoLastOperation() {
	if (redoHistory.empty()) return;

	PaintOperation paintOp = redoHistory.front();
	AddUndoOperation(paintOp.ExecutePaint());
	redoHistory.pop_front();
}

// Paint Methods
//********************************
void Paint(PaintOperation& paintOp, BlockInfo newBlock, CoordinateInBlocks At) {
	paintOp.paintedBlocks.push_back(
		Block(GetAndSetBlock(At, newBlock), At)
	);
}

bool MarkersInLoadedChunks() {
	if (!GetBlock(marker1Cord).IsValid()) {
		SpawnHintText(
			GetPlayerLocation(),
			L"Marker 1 Not Found",
			1,
			1,
			1
		);
		return false;
	}
	if (!GetBlock(marker2Cord).IsValid()) {
		SpawnHintText(
			GetPlayerLocation(),
			L"Marker 2 Not Found",
			1,
			1,
			1
		);
		return false;
	}
	return true;
}

BlockInfo SetPaintTarget() {
	if (!GetBlock(paintCord).IsValid()) {
		SpawnHintText(
			GetPlayerLocation(),
			L"Paint Block Not Found",
			1,
			1,
			1
		);
		return BlockInfo(EBlockType::Invalid);
	}
	else {
		return GetBlock(CoordinateInBlocks(paintCord.X, paintCord.Y, paintCord.Z + 1));
	}
}

void PaintArea() {
	bool useMask = false;
	std::vector<BlockInfo> maskBlocks;
	PaintOperation paintOp;

	if (!MarkersInLoadedChunks()) return;

	BlockInfo targetBlock = SetPaintTarget();
	if (!targetBlock.IsValid()) return;

	// Set the block mask if one exists
	if ((GetBlock(maskCord).IsValid())) {
		maskBlocks = GetMaskBlocks();
		if (!(maskBlocks.empty())) {
			useMask = true;
		}
	}

	CoordinateInBlocks startCorner = GetSmallVector(marker1Cord, marker2Cord);
	CoordinateInBlocks endCorner = GetLargeVector(marker1Cord, marker2Cord);

	for (int16_t z = startCorner.Z; z <= endCorner.Z; z++) {

		for (int64_t y = startCorner.Y; y <= endCorner.Y; y++) {

			for (int64_t x = startCorner.X; x <= endCorner.X; x++) {
				
				BlockInfo currentBlock = GetBlock(CoordinateInBlocks(x, y, z));
				if (useMask && !(BlockIsMaskTarget(currentBlock, maskBlocks)) ) {
					continue;
				}
				Paint(paintOp, targetBlock, CoordinateInBlocks(x, y, z));
			}
		}
	}
	AddUndoOperation(paintOp);
}

// Clipboard Method
//********************************
void CopyRegion() {
	CoordinateInBlocks startCorner = GetSmallVector(marker1Cord, marker2Cord);
	CoordinateInBlocks endCorner = GetLargeVector(marker1Cord, marker2Cord);
	clipboard.clear();

	clipboardWidth = endCorner.X - startCorner.X;
	clipboardLength = endCorner.Y - startCorner.Y;

	for (int16_t z = startCorner.Z; z <= endCorner.Z; z++) {

		for (int64_t y = startCorner.Y; y <= endCorner.Y; y++) {

			for (int64_t x = startCorner.X; x <= endCorner.X; x++) {
				int64_t localX = x - startCorner.X;
				int64_t localY = y - startCorner.Y;
				int16_t localZ = z - startCorner.Z;
				BlockInfo currentBlock = GetBlock(CoordinateInBlocks(x, y, z));

				clipboard.push_back(Block(currentBlock, CoordinateInBlocks(localX, localY, localZ)));
			}
		}
	}
}

void CutRegion() {
	PaintOperation paintOp;

	CoordinateInBlocks startCorner = GetSmallVector(marker1Cord, marker2Cord);
	CoordinateInBlocks endCorner = GetLargeVector(marker1Cord, marker2Cord);
	
	clipboard.clear();

	clipboardWidth = endCorner.X - startCorner.X;
	clipboardLength = endCorner.Y - startCorner.Y;

	for (int16_t z = startCorner.Z; z <= endCorner.Z; z++) {

		for (int64_t y = startCorner.Y; y <= endCorner.Y; y++) {

			for (int64_t x = startCorner.X; x <= endCorner.X; x++) {
				int64_t localX = x - startCorner.X;
				int64_t localY = y - startCorner.Y;
				int16_t localZ = z - startCorner.Z;
				BlockInfo currentBlock = GetBlock(CoordinateInBlocks(x, y, z));

				clipboard.push_back(Block(currentBlock, CoordinateInBlocks(localX, localY, localZ)));
				Paint(paintOp, EBlockType::Air, CoordinateInBlocks(x, y, z));
			}
		}
	}
	AddUndoOperation(paintOp);
}

void PasteClipboard(CoordinateInBlocks At) {
	if (clipboard.empty()) return;
	PaintOperation paintOp;

	bool ignoreAirBlocks = false;
	BlockInfo blockAbove = GetBlock(GetBlockAbove(At));
	if (blockAbove.CustomBlockID == AirFilter) {
		ignoreAirBlocks = true;
	}

	for (int i = 0; i <= clipboard.size(); i++) {
		if (ignoreAirBlocks && clipboard[i].blockInfo.Type == EBlockType::Air) continue;

		Paint(paintOp, clipboard[i].blockInfo, At + clipboard[i].location);
	}
	AddUndoOperation(paintOp);
}

void RotateClipboard90DegreesClockwise() {
	for (int i = 0; i <= clipboard.size(); i++) {
		int64_t x = clipboard[i].location.Y;
		int64_t y = clipboard[i].location.X * -1;
		clipboard[i].location = CoordinateInBlocks(x, y+clipboardWidth, clipboard[i].location.Z);
	}
	int64_t width = clipboardWidth;
	clipboardWidth = clipboardLength;
	clipboardLength = width;
}

void RotateClipboard90DegreesCounterClockwise() {
	for (int i = 0; i <= clipboard.size(); i++) {
		int64_t x = clipboard[i].location.Y * -1;
		int64_t y = clipboard[i].location.X;
		clipboard[i].location = CoordinateInBlocks(x+clipboardLength, y, clipboard[i].location.Z);
	}
	int64_t width = clipboardWidth;
	clipboardWidth = clipboardLength;
	clipboardLength = width;
}

/************************************************************* 
//	Event Functions
*************************************************************/

void Event_BlockPlaced(CoordinateInBlocks At, UniqueID CustomBlockID, bool Moved)
{
	if (CustomBlockID == Marker1Block) {
		marker1Cord = At;
	}
	else if (CustomBlockID == Marker2Block) {
		marker2Cord = At;
	}
	else if (CustomBlockID == MaskBlock) {
		maskCord = At;
	}
	else if (CustomBlockID == PaintBlock) {
		paintCord = At;
	}
}

void Event_BlockDestroyed(CoordinateInBlocks At, UniqueID CustomBlockID, bool Moved)
{
	if (CustomBlockID == MaskBlock) {
		// May cause issues in edge case if painting after placing multiple palettes
		maskCord = CoordinateInBlocks(0, 0, 0);
	}
	if (CustomBlockID == PaletteBlock) {
		RemovePalette(At);
	}
}

void Event_BlockHitByTool(CoordinateInBlocks At, UniqueID CustomBlockID, wString ToolName, CoordinateInCentimeters ExactHitLocation, bool ToolHeldByHandLeft)
{
	if (ToolName == L"T_Arrow") {
		if (CustomBlockID == PasteBlock) {
			PasteClipboard(At);
		}
	}

	if (ToolName == L"T_Stick") {
		if (CustomBlockID == PaletteBlock) {
			BlockInfo painterBlock = GetBlock(At + CoordinateInBlocks(1, 0, 0));
			if (painterBlock.CustomBlockID == PaintBlock) {
				RemovePalette(At);
			}
			else {
				TryGeneratePalette(At);
			}
		}
		else if (CustomBlockID == PaintBlock) {
			PaintArea();
			SpawnHintText(GetBlockAbove(At), L"Painting Area.", 1, 1);
		}
		else if (CustomBlockID == UndoBlock) {
			UndoLastOperation();
			SpawnHintText(GetBlockAbove(At), L"Undoing Last Operation", 1, 1);
		}
		else if (CustomBlockID == RedoBlock) {
			RedoLastOperation();
			SpawnHintText(GetBlockAbove(At), L"Redoing Last Operation.", 1, 1);
		}
		else if (CustomBlockID == CopyBlock) {
			CopyRegion();
			SpawnHintText(GetBlockAbove(At), L"Copying Selected Region.", 1, 1);
		}
		else if (CustomBlockID == CutBlock) {
			CutRegion();
			SpawnHintText(GetBlockAbove(At), L"Cutting Selected Region.", 1, 1);
		}
		else if (CustomBlockID == ToggleWandBlock) {
			selectionWandEnabled = !selectionWandEnabled;
			if (selectionWandEnabled) exchangingWandEnabled = false;
			wString messageText = (selectionWandEnabled) ? L"Selection Wand Enabled" : L"Selection Wand Disabled";
			SpawnHintText(GetBlockAbove(At), messageText, 1, 1);
		}
		else if (CustomBlockID == ToggleExchangeBlock) {
			exchangingWandEnabled = !exchangingWandEnabled;
			if (exchangingWandEnabled) selectionWandEnabled = false;
			wString messageText = (exchangingWandEnabled) ? L"Exchanging Wand Enabled" : L"Exchanging Wand Disabled";
			SpawnHintText(GetBlockAbove(At), messageText, 1, 1);
		}
		else if (CustomBlockID == Rotate90CWBlock) {
			RotateClipboard90DegreesClockwise();
			SpawnHintText(GetBlockAbove(At), L"Rotating Clipboard 90 degrees clockwise.", 1, 1);
		}
		else if (CustomBlockID == Rotate90CCWBlock) {
			RotateClipboard90DegreesCounterClockwise();
			SpawnHintText(GetBlockAbove(At), L"Rotating Clipboard 90 degrees counterclockwise", 1, 1);
		}
		else if (CustomBlockID == PasteBlock) {
			PasteClipboard(At);
		}
	}
}

void Event_Tick()
{

}

void Event_OnLoad()
{

}

void Event_OnExit()
{
	
}

/*************************************************************
//	Advanced Event Functions
*************************************************************/

void Event_AnyBlockPlaced(CoordinateInBlocks At, BlockInfo Type, bool Moved)
{

}

void Event_AnyBlockDestroyed(CoordinateInBlocks At, BlockInfo Type, bool Moved)
{

}

void Event_AnyBlockHitByTool(CoordinateInBlocks At, BlockInfo Type, wString ToolName, CoordinateInCentimeters ExactHitLocation, bool ToolHeldByHandLeft)
{
	if (exchangingWandEnabled) {
		if (ToolName == L"T_Arrow") {
			exchangeTarget = Type;
		}
		if (ToolName == L"T_Pickaxe_Stone" || ToolName == L"T_Axe_Stone") {
			SetBlock(At, exchangeTarget);
		}
	}

	if (selectionWandEnabled) {
		if (ToolName == L"T_Pickaxe_Stone") {
			SpawnHintText(At + CoordinateInBlocks(0, 0, 1), L"Marker 1 set!", 1, 1);
			marker1Cord = At;
		}
		if (ToolName == L"T_Axe_Stone") {
			SpawnHintText(At + CoordinateInBlocks(0, 0, 1), L"Marker 2 set!", 1, 1);
			marker2Cord = At;
		}
	}
}