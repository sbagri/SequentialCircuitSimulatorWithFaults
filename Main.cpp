#include <iostream>
#include <stdio.h>
#include <string.h>
#include <cstdlib>

#define CKT_NAME_CHARS	40
#define	HIGHEST_LVL	100
#define	NOT_END	0
#define END		1

//#define DEBUG		//Prints event queue, gate values, computation at each gate, fanout gates
//#define DEBUG2	//for fault free simulation, prints output as in .out file
//#define DEBUG4	//prints as in DEBUG2 in faulty simulation case

using namespace std;

enum
{
   G_JUNK,         /* 0 */
   G_INPUT,        /* 1 */
   G_OUTPUT,       /* 2 */
   G_XOR,          /* 3 */
   G_XNOR,         /* 4 */
   G_DFF,          /* 5 */
   G_AND,          /* 6 */
   G_NAND,         /* 7 */
   G_OR,           /* 8 */
   G_NOR,          /* 9 */
   G_NOT,          /* 10 */
   G_BUF,          /* 11 */
};

class circuit
{
    // circuit information
public:
	unsigned int numgates;		// total number of gates (faulty included)
	unsigned int maxlevels;		// number of levels in gate level ckt in increments of 5
	unsigned int max_level1s;	// number of levels in gate level ckt in increments of 1
	unsigned int numInputs;		// number of inputs
	unsigned int numOutputs;	// number of outputs
	unsigned int numDFFs;

	unsigned int *gtype;		// gate type
	unsigned int *fanin;		// number of fanins of gate
	unsigned int *fanout;		// number of fanouts of gate
	unsigned int *levelNum;		// level number of gate
	unsigned int **faninlist;	// fanin list of each gate
	unsigned int **fanoutlist;	// fanout list of each gate
	unsigned int **c_0_1;		// c0 and c1 of each gate
	circuit(char *);	// constructor

	FILE *Out_Sfile;		//print <ckt>_S.out file
	FILE *Ufl_Sfile;		//print <ckt>_S.ufl file
	FILE *Det_Sfile;		//print <ckt>_S.det file

	unsigned int *DFFindex;		// gate no. of DFFs
	unsigned int NumVec;		// Total no. of vectors in .vec file (between 'a number' and "END")
	unsigned int NoBitsEachVec;	// No. of bits in each vector
	int **Vects;				// save each line of .vec file, read in same order
								
	
	bool PrtCkt();				//	prints .lev file, used for debugging
	bool ReadVecFile(char *);	//	reads .vec file, read all sequences and all vectors inside each sequence
	bool PrtVecFile();			//	prints .vec file

	int *PrevVal;			//values are saved as per gate num, i.e from index 1, values of each gate during simulation
	int *NewVal;			//values are saved as per gate num, i.e from index 1, values of each gate during simulation
	bool UpdatePrevNew();	//at end of cycle update new to previous
	bool PrintPrev() const;	
	bool PrintNew() const;
	bool PrintGateNo() const;	// to be used before printnew to get which gate no. it is printing values for
	bool PrintInput() const;
	bool PrintInput_outs();		//Prints in _S.out file
	bool PrintOutput() const;
	bool PrintOutput_outs();	//Prints in _S.out file
	bool reset(int);		//reset newval and prevval array

	int *FFValBuf;			//Value stored in a particular FF, index is from 0 to num of flip flops, not by gatenum
	int **FltFreeOpVctrs;	// reference output, 1st index is vector no, 2nd index is output no.
	int **FltyOpVctrs;		// place to save output vector with each fault simulation
	bool GetNewVect(unsigned int);	//get next vector to simulate
	bool SaveFFVal();				//Save FF val to update in next cycle, done normally at end of simulation
									//except when FF itself is faulty
	bool UpdateFFs();				
	bool PrtFFState();
	bool PrtFFState_outs(bool);
	bool ResetFFVal();				//Reset FF state after each sequence simulation
	bool GetFltFreeOpVctrs(unsigned int);	//Save Fault free ouptut vectors
	bool GetFltyOpVctrs(unsigned int);		//not used as comparison is directly done with output, this can be deleted
	unsigned int CmprFltyOps(unsigned int);	//Compare faulty and fault free output, return 2 if difference found

//variable es_* is variable related to event driven simulation
	bool ReadEqfFile(char *);	// read .eqf file
	unsigned int TotNumFlt;		// Total Num of faults
	unsigned int **FltVct;		// First fault of each line of .eqf file, saved in same order
								// i.e FltVct[FltNo][0] = gatenum, [1] = which ip/op stuck at, [2] = stuck at value
	bool PrtEqfFile();
	unsigned int FltNo;			// Current Fault no. on which simulation is happening
	bool DoFaultySim;			// 1-> simulate with fault else faultless simulation

	unsigned int es_thislevel;	// Current level on which working
	unsigned int *es_index;		// in Event queue, saves how many entries are there per row
	unsigned int **es_EvtQueue;	// actual Event queue, each row is a level
	unsigned int es_MaxEvtAtLevel;	//Maximum possible event level
	unsigned int es_highestEvtLevel;	//higest level which has any event
	unsigned int es_thisGateNum;	//Gate no. of current gate being simulated
	bool ResetEvtQueue();			//save 0 to all cells of es_EvtQueue
	bool PrtEvtQueue() const;		//Print event queue
	bool UpdateEventList();			//see fanout of current gate and if its value changed then schedule
									//all in Event queue
	bool SimOneCyc();				//Simulate one clock cycle of entire circuit
	bool SimFaultFree();			//Simulate the fault free gate 
	bool SimFaulty();				//Simulate the faulty gate


};

circuit::circuit(char *cktName)
{
    FILE *inFile;
    char fName[CKT_NAME_CHARS];
    unsigned int count, junk, i, gatenum, f1, f2, f3, j; 
	char c;
    
    strcpy(fName, cktName);
	strcat(fName, ".lev");
    inFile = fopen(fName, "r");
    if (inFile == NULL)
    {
		fprintf(stderr, "Can't open .lev file\n");
		exit(-1);
    }

    numgates = maxlevels = max_level1s = 0;
    numInputs = numOutputs = 0;
	numDFFs = 0;

    fscanf(inFile, "%d", &count);	// number of gates
    fscanf(inFile, "%d", &junk);	// skip the second line

    // allocate space for gates data structure
    gtype = new unsigned int[count];
    fanin = new unsigned int[count];
    fanout = new unsigned int[count];
    levelNum = new unsigned int[count];
    faninlist = new unsigned int * [count];
    fanoutlist = new unsigned int * [count];
	c_0_1 = new unsigned int * [count];
	PrevVal = new int[count];
	NewVal = new int[count];

    // now read in the circuit
    for (i=1; i<count; i++)
    {
		fscanf(inFile, "%d", &gatenum);
		fscanf(inFile, "%d", &f1);
		fscanf(inFile, "%d", &f2);
		fscanf(inFile, "%d", &f3);

		numgates++;
		gtype[gatenum] = (unsigned int) f1;
		if (gtype[gatenum] > 13 || gtype[gatenum] < 1)
		{	
			printf("gate %d is an unimplemented gate type\n", gatenum);
			exit(20);
		}
		switch(gtype[gatenum])
		{	
			case G_INPUT:	numInputs++;	break;
			case G_OUTPUT:	numOutputs++;	break;
			case G_DFF	:	numDFFs++;		break;
		}

		f2 = (unsigned int) f2;
		levelNum[gatenum] = f2;

		if (f2 >= (maxlevels))
			maxlevels = f2 + 5;

		fanin[gatenum] = (unsigned int) f3;
		// now read in the faninlist
		faninlist[gatenum] = new unsigned int[fanin[gatenum]];
		for (j=0; j<fanin[gatenum]; j++)
		{
			fscanf(inFile, "%d", &f1);
			faninlist[gatenum][j] = (unsigned int) f1;
		}

		for (j=0; j<fanin[gatenum]; j++) // followed by samethings
			fscanf(inFile, "%d", &junk);

		// read in the fanout list
		fscanf(inFile, "%d", &f1);
		fanout[gatenum] = (unsigned int) f1;

		fanoutlist[gatenum] = new unsigned int[fanout[gatenum]];
		for (j=0; j<fanout[gatenum]; j++)
		{
			fscanf(inFile, "%d", &f1);
			fanoutlist[gatenum][j] = (unsigned int) f1;
		}
	
		while((c = getc(inFile)))
			{if ((c == ';') || (c=='O'))	break;
			}
	
		c_0_1[gatenum] = new unsigned int[2];
		fscanf(inFile, "%d", &f1);
		c_0_1[gatenum][0] = (unsigned int) f1;
		fscanf(inFile, "%d", &f2);
		c_0_1[gatenum][1] = (unsigned int) f2;

		PrevVal[gatenum] = -1;
		NewVal[gatenum] = -1;
		// skip till end of line
		while ((c = getc(inFile)) != '\n' && c != EOF)
			;
		}
		fclose(inFile);
	
		max_level1s = maxlevels/5;
		//can be removed as its dynamic now
		if(max_level1s > HIGHEST_LVL)
		{
			fprintf(stderr, "Array path's size is smaller than maximum level, increase HIGHEST_LVL value in code\n");
			exit(21);
		}

		es_index = new unsigned int[max_level1s];

		for(i=0; i<max_level1s; i++)
		{	
			es_index[i] = 0;
		}
		//trying to get MAX_EVENTS_AT_ANY_LEVEL, useful to intialize 2nd dimension of es_EvtQueue
		//taking one more than max
		es_MaxEvtAtLevel = 1;
		for (i=1; i<count; i++)
		{	es_index[(levelNum[i]/5)]++;
		}

		for(i=0; i<max_level1s; i++)
		{	if(es_index[i] > es_MaxEvtAtLevel)
			{	es_MaxEvtAtLevel = es_index[i];
			}
			//resetting es_index, so that it can be used for the purpose it was created	
			//just used it here to save creating another array
			es_index[i] = 0;
		}

		es_EvtQueue = new unsigned int * [max_level1s];
		for(i=0; i<max_level1s; i++)
		{
			es_EvtQueue[i] = new unsigned int[es_MaxEvtAtLevel];
		}
	
		if(numDFFs > 0)
		{
			DFFindex = new unsigned int[numDFFs];
			FFValBuf = new int[numDFFs];
			j = 0;
		
			for (i=1; i<count; i++)
			{	
				if(G_DFF == gtype[i])
				{
					DFFindex[j] = i;
					FFValBuf[j++] = -1;
				}
			}
			if(j != numDFFs)
			{	cout<<"Problem in updating DFFindex array"<<endl;
			}
		}
		
		//creating all files to be printed at end of simulation
		memset(fName, 0, CKT_NAME_CHARS);
		strcpy(fName, cktName);
		strcat(fName, "_S.out");
		Out_Sfile = fopen(fName, "w");
		if (Out_Sfile == NULL)
		{
			fprintf(stderr, "Can't open _S.out file\n");
			exit(23);
		}

		memset(fName, 0, CKT_NAME_CHARS);
		strcpy(fName, cktName);
		strcat(fName, "_S.ufl");
		Ufl_Sfile = fopen(fName, "w");
		if (Ufl_Sfile == NULL)
		{
			fprintf(stderr, "Can't open _S.ufl file\n");
			exit(23);
		}

		memset(fName, 0, CKT_NAME_CHARS);
		strcpy(fName, cktName);
		strcat(fName, "_S.det");
		Det_Sfile = fopen(fName, "w");
		if (Det_Sfile == NULL)
		{
			fprintf(stderr, "Can't open _S.out file\n");
			exit(23);
		}
	
		fprintf(Out_Sfile, "Successfully read in circuit:\n");
		fprintf(Out_Sfile, "\t%d PIs.\n", numInputs);
		fprintf(Out_Sfile, "\t%d POs.\n", numOutputs);
		fprintf(Out_Sfile, "\t%d Dffs.\n", numDFFs);
		fprintf(Out_Sfile, "\t%d total number of gates\n", (numgates+1));
		fprintf(Out_Sfile, "\t%d levels in the circuit.\n", maxlevels / 5);
}

bool circuit::PrtCkt()
{	
	unsigned int i, j;
	cout<<numgates+1<<endl;
	cout<<numgates+1<<endl;
	for(i=1; i<=numgates; i++)
	{
		cout<<i<<" "<<gtype[i]<<" "<<levelNum[i]<<" "<<fanin[i]<<" ";
		for(j=0; j<fanin[i]; j++)
			cout<<faninlist[i][j]<<" ";
		for(j=0; j<fanin[i]; j++)
			cout<<faninlist[i][j]<<" ";
		cout<<fanout[i]<<" ";
		for(j=0; j<fanout[i]; j++)
			cout<<fanoutlist[i][j]<<" ";
		cout<<"; "<<c_0_1[i][0]<<" "<<c_0_1[i][1]<<endl;
	}
	return 1;
}

//taking circuit name as input again so that other 
//vector files can also be passed with different names
bool circuit::ReadVecFile(char *cktName)
{
	FILE *inFile;
	char fName[CKT_NAME_CHARS];
	char junk[CKT_NAME_CHARS];
	NumVec = 0;
	char f1 = '\0';
		
	strcpy(fName, cktName);
	strcat(fName, ".vec");
	inFile = fopen(fName, "r");
	if (inFile == NULL)
    {
		fprintf(stderr, "Can't open .vec file\n");
		cout<<".vec file doesn't exists"<<endl;
		return 0;
    }
	//reading first line which denotes no. of bits in each vector
	fscanf(inFile, "%d", &NoBitsEachVec);
	fgets(junk, CKT_NAME_CHARS, inFile);
	
	while(NULL != fgets(junk, CKT_NAME_CHARS, inFile))
	{	
		if(1 == strcmp(junk, "END"))
		{	break;
		}
		NumVec++;
	}

	fseek ( inFile , 0 , SEEK_SET );
	fgets(junk, CKT_NAME_CHARS, inFile);
	
	Vects = new int * [NumVec];

	for(unsigned int i=0; i<NumVec; i++)
	{	Vects[i] = new int[NoBitsEachVec];
		for(unsigned int j=0; j<NoBitsEachVec; j++)
		{
			fscanf(inFile, "%c", &f1);
			Vects[i][j] = (int)atoi(&f1);
		}
		while ((f1 = getc(inFile)) != '\n');
	}
	
	//Making place for saving Fault free and faulty outputs
	FltFreeOpVctrs = new int * [NumVec];
	for(unsigned int i=0; i<NumVec; i++)
	{
		FltFreeOpVctrs[i] = new int[numOutputs];
		for(unsigned int j=0; j<numOutputs; j++)
		{	FltFreeOpVctrs[i][j] = -1;
		}
	}

	//if fault simulation is enabled then make place to save faulty outputs also
	if(1 == DoFaultySim)
	{	
		FltyOpVctrs = new int * [NumVec];
		for(unsigned int i=0; i<NumVec; i++)
		{
			FltyOpVctrs[i] = new int[numOutputs];
			for(unsigned int j=0; j<numOutputs; j++)
			{	FltyOpVctrs[i][j] = -1;
			}
		}
	}

	fclose(inFile);
	return 1;
}

bool circuit::PrtVecFile()
{	cout<<NoBitsEachVec<<endl;
	for(unsigned int i=0; i<NumVec; i++)
	{
		for(unsigned int j=0; j<NoBitsEachVec; j++)
		{
			cout<<Vects[i][j];
		}
		cout<<endl;
	}
	return 1;
}

bool circuit::UpdatePrevNew()
{
	for(unsigned i=1;i<=numgates;i++)
		PrevVal[i] = NewVal[i];
	return 1;	
}

bool circuit::PrintPrev() const
{
	for(unsigned int i=1; i<=numgates; i++)
	{	if(-1 == PrevVal[i])
		{
			printf("X");
		}
		else
		{
			printf("%d",PrevVal[i]);
		}
	}
	printf("\n");
	return 1;
}

//only print known values, so that don't have to look on the full big list everytime
bool circuit::PrintNew() const
{
	PrintGateNo();
	for(unsigned int i=1;i<=numgates;i++)
		{	if(-1 == NewVal[i])
			{
				//printf("  X");
			}
			else
			{
				printf("%4d",NewVal[i]);
			}
		}
	printf("\n");
	return 1;
}

bool circuit::PrintGateNo() const
{
	for(unsigned int i=1;i<=numgates;i++)
	{	if(-1 != NewVal[i])
			printf("%4d",i);
	}
	printf("\n");
	return 1;
}

bool circuit::PrintInput() const
{
	for(unsigned int i=1;i<=numInputs;i++)
	{	if(-1 == NewVal[i])
		{	
			printf("X");
		}
		else
		{
			printf("%d",NewVal[i]);
		}
	}
	printf("\n");
	return 1;	
}

bool circuit::PrintInput_outs()
{	fprintf(Out_Sfile, "INPUT: ");
	for(unsigned int i=1;i<=numInputs;i++)
	{	if(-1 == NewVal[i])
		{	
			fprintf(Out_Sfile, "X");
		}
		else
		{
			fprintf(Out_Sfile, "%d",NewVal[i]);
		}
	}
	fprintf(Out_Sfile, "\n");
	return 1;	
}

bool circuit::PrintOutput() const
{	
	for(unsigned int i=1;i<=numgates;i++)
	{	if(G_OUTPUT == gtype[i])
		{	
			if(-1 == NewVal[i])
			{	
				printf("X");
			}
			else
			{
				printf("%d",NewVal[i]);
			}
		}
	}
	printf("\n");
	return 1;	
}

bool circuit::PrintOutput_outs()
{	fprintf(Out_Sfile, "OUTPUT: ");
	for(unsigned int i=1;i<=numgates;i++)
	{	if(G_OUTPUT == gtype[i])
		{	
			if(-1 == NewVal[i])
			{	
				fprintf(Out_Sfile, "X");
			}
			else
			{
				fprintf(Out_Sfile, "%d",NewVal[i]);
			}
		}
	}
	fprintf(Out_Sfile, "\n");
	return 1;	

}

bool circuit::reset(int ResetVal)
{
	for(unsigned int i=1;i<=numgates;i++)
	{	NewVal[i] = ResetVal;
		PrevVal[i] = ResetVal;
	}
	return 1;
}

bool circuit::GetNewVect(unsigned int VectNo)
{	//assuming inputs would only be at beginning of .lev file
	for(unsigned int i=1; i<=numInputs; i++)
	{	
		NewVal[i] = Vects[VectNo][(i-1)];
	}
	return 1;
}

bool circuit::SaveFFVal()
{
	for(unsigned int i=0; i<numDFFs; i++)
	{	if(-1 != NewVal[faninlist[DFFindex[i]][0]])	
		{	//save new value only if its not "unknown"
			if(1 != DoFaultySim)
			{	FFValBuf[i] = NewVal[faninlist[DFFindex[i]][0]];
			}
			else
			{	if(DFFindex[i] == FltVct[FltNo][0] && FltVct[FltNo][1] == 1)
				{	//meaning fault is on input of DFF
					#ifdef DEBUG
						cout<<"Input of DFF gate "<<DFFindex[i]<<" is faulty, so updating FF with "<<FltVct[FltNo][2]<<endl;
					#endif
					FFValBuf[i] = FltVct[FltNo][2];
				}
				else
				{	FFValBuf[i] = NewVal[faninlist[DFFindex[i]][0]];
				}
			}
		}
	}
	return 1;
}

bool circuit::UpdateFFs()
{	unsigned int FFGateNo;
	for(unsigned int i=0; i<numDFFs; i++)
	{	FFGateNo = DFFindex[i];
		NewVal[FFGateNo] = FFValBuf[i];
		//resetting this buffer so that they don't get carried over to next simulation 
		FFValBuf[i] = -1;
	}
	return 1;
}

bool circuit::PrtFFState()
{	
	for(unsigned int i=0; i<numDFFs; i++)
	{	if(-1 == NewVal[DFFindex[i]])
		{
			printf("X");
		}
		else
		{
			printf("%d",NewVal[DFFindex[i]]);
		}
	}
	printf("\n");
	return 1;
}

bool circuit::PrtFFState_outs(bool end)
{	if(0 == end)
		fprintf(Out_Sfile, "PRESENT STATE: ");
	else
		fprintf(Out_Sfile, "FINAL Next State: ");

	for(unsigned int i=0; i<numDFFs; i++)
	{	if(-1 == NewVal[DFFindex[i]])
		{
			fprintf(Out_Sfile, "X");
		}
		else
		{
			fprintf(Out_Sfile, "%d",NewVal[DFFindex[i]]);
		}
	}
	fprintf(Out_Sfile, "\n");
	return 1;
}

bool circuit::ResetFFVal()
{
	for(unsigned int i=0; i<numDFFs; i++)
		FFValBuf[i] = -1;
	return 1;
}

bool circuit::GetFltFreeOpVctrs(unsigned int VectNo)
{	unsigned int j=0;
	for(unsigned int i=1; i<=numgates; i++)
	{	if(G_OUTPUT == gtype[i])
		{	FltFreeOpVctrs[VectNo][j++] = NewVal[i];
			#ifdef DEBUG
			if(j > numOutputs)
				cout<<"Possibly some fault in updating FltFreeOpVctrs"<<endl;
			#endif
		}
	}
	return 1;
}

bool circuit::GetFltyOpVctrs(unsigned int VectNo)
{
	return 1;
}

unsigned int circuit::CmprFltyOps(unsigned int VectNo)
{	unsigned int j=0;
	for(unsigned int i=1; i<=numgates; i++)
	{	if(G_OUTPUT == gtype[i])
		{	if((-1 != FltFreeOpVctrs[VectNo][j]) && (-1 !=NewVal[i])  && (FltFreeOpVctrs[VectNo][j] != NewVal[i]))
			{//	cout<<"Fault Detected as output mismatches at Gate No "<<i<<endl;
				j++;
				return 2;
			}
			else j++;
		}
	}
	return 1;
}

bool circuit::ReadEqfFile(char *cktName)
{
	FILE *inFile;
	char fName[CKT_NAME_CHARS];
	char junk[CKT_NAME_CHARS*100];
	TotNumFlt = 0;
	
	strcpy(fName, cktName);
	strcat(fName, ".eqf");
	inFile = fopen(fName, "r");
	if (inFile == NULL)
    {
		fprintf(stderr, "Can't open .eqf file\n");
		cout<<".eqf file doesn't exists"<<endl;
		return 0;
    }

	//Reading Total lines of fault
	while(NULL != fgets(junk, CKT_NAME_CHARS*100, inFile))
	{	
		//in case each fault is to be counted, counted no. of ':' in file
		//and add 1 for each line or add count of ';'
		TotNumFlt++;
	}

	FltVct = new unsigned int * [TotNumFlt];

	fseek ( inFile , 0 , SEEK_SET );

	unsigned int GatNo, FaultLoc, SAval;
	char c;

	for(unsigned int i=0; i<TotNumFlt; i++)
	{
		FltVct[i] = new unsigned int[3];
		fscanf(inFile, "%d", &GatNo);
		FltVct[i][0] = (unsigned int)GatNo;
		fscanf(inFile, "%d", &FaultLoc);
		FltVct[i][1] = (unsigned int)FaultLoc;
		fscanf(inFile, "%d", &SAval);
		FltVct[i][2] = (unsigned int)SAval;
		
		while ((c = getc(inFile)))
		{	if(c == '\n' || c==EOF /*|| c==';'*/)
			{	//commented last condition as some lines have RED written at end
				break;
			}
		}
	}
	return 1;
}

bool circuit::PrtEqfFile()
{
	for(unsigned int i=0; i<TotNumFlt; i++)
		cout<<FltVct[i][0]<<" "<<FltVct[i][1]<<" "<<FltVct[i][2]<<endl;
	return 1;
}

bool circuit::ResetEvtQueue()
{
	for(unsigned int i=0; i<max_level1s; i++)
	{	for(unsigned int j=0; j<es_MaxEvtAtLevel; j++)
		{	es_EvtQueue[i][j] = 0;
		}
		es_index[i] = 0;
	}
	es_highestEvtLevel = 0;
	es_thislevel = 0;
	es_thisGateNum = 0;
	return 1;
}

bool circuit::PrtEvtQueue() const
{
	for(unsigned int i=0; i<max_level1s; i++)
	{	for(unsigned int j=0; j<es_MaxEvtAtLevel; j++)
		{	cout<<es_EvtQueue[i][j]<<" ";
		}
		cout<<endl;
	}
	return 1;
}

bool circuit::UpdateEventList()
{
	bool Flag_FoundInEvtQueue;
	unsigned int LevelOfOut;
	unsigned int l = 0;
	//schedule fanouts of a gate only if that gate's output is not X
	if(-1 != NewVal[es_thisGateNum])
	{
		#ifdef DEBUG			
			cout<<"Fanout = ";
		#endif
		for(unsigned int k=0; k<fanout[es_thisGateNum]; k++)
		{	
			#ifdef DEBUG			
				cout<<fanoutlist[es_thisGateNum][k]<<" ";
			#endif
			Flag_FoundInEvtQueue=0;
			LevelOfOut = (levelNum[fanoutlist[es_thisGateNum][k]] / 5);
			for(l=0; l<es_index[LevelOfOut]; l++)
			{	if(fanoutlist[es_thisGateNum][k] == es_EvtQueue[LevelOfOut][l])
				{	Flag_FoundInEvtQueue=1;
					break;
				}
			}
	
			if(0 == Flag_FoundInEvtQueue)
			{	//Add to event queue
				es_EvtQueue[LevelOfOut][l] = fanoutlist[es_thisGateNum][k];
				es_index[LevelOfOut]++;
				if(LevelOfOut > es_highestEvtLevel)
				{	es_highestEvtLevel = LevelOfOut;
				}
			}
		}
		#ifdef DEBUG
			cout<<endl;
		#endif
	}
	#ifdef DEBUG
		PrtEvtQueue();
	#endif
	return 1;
}

bool circuit::SimOneCyc()
{
	ResetEvtQueue();
	for(unsigned int i=1;i<=numgates;i++)
	{
		if (NewVal[i] != PrevVal[i])
		{	//Add to event queue only if new val != prev val
			es_thislevel = levelNum[i]/5;
			es_EvtQueue[es_thislevel][es_index[es_thislevel]] = i;
			es_index[es_thislevel]++;
			if(es_thislevel > es_highestEvtLevel)
			{	es_highestEvtLevel = es_thislevel;
			}
		}
	}
	if(1 == DoFaultySim)	//schedule a faulty gate if its not already scheduled
	{	bool flag_faultyGateInEvtQueue = 0;
		es_thislevel = levelNum[FltVct[FltNo][0]] / 5;
		for(unsigned int j=0; j<es_index[es_thislevel]; j++)
		{	if(FltVct[FltNo][0] == es_EvtQueue[es_thislevel][j])
			{	flag_faultyGateInEvtQueue = 1;
				break;
			}
		}
		if(0 == flag_faultyGateInEvtQueue)	//faulty gate not in event queue, adding it to queue
		{	es_EvtQueue[es_thislevel][es_index[es_thislevel]] = FltVct[FltNo][0];
			es_index[es_thislevel]++;
			if(es_thislevel > es_highestEvtLevel)
			{	es_highestEvtLevel = es_thislevel;
			}
		}
	}
	#ifdef DEBUG
		circuit::PrtEvtQueue();
	#endif

	//es_highestEvtLevel would be updated inside only if event vector gets an event of higher level
	//after processing the current events in queue

	for(unsigned int i=0; i<=es_highestEvtLevel; i++)
	{
		for(unsigned int j=0; j<es_index[i]; j++)
		{	
			es_thisGateNum = es_EvtQueue[i][j];
			if((es_thisGateNum < 1) || (es_thisGateNum > numgates))
			{	printf("Not a valid gate on Event queue[%d][%d] gate num is %d\n", i, j, es_thisGateNum);
				exit(22);	//not a valid gate on Event Queue
			}
			if(1 == DoFaultySim)
			{	if(es_thisGateNum == FltVct[FltNo][0])
				{	SimFaulty();
				}
				else
				{	SimFaultFree();
				}
			}
			else
			{	SimFaultFree();
			}
		}
	}
	return 1;
}

bool circuit::SimFaultFree()
{
	int InterResult = -1;
	#ifdef DEBUG
		cout<<"evaluating "<<es_thisGateNum;
	#endif

	switch(gtype[es_thisGateNum])
	{
		case G_JUNK/*0*/:	printf("Gate Type: G_JUNK\n");	break;
		case G_INPUT/*1*/:	
			#ifdef DEBUG
				cout<<" INPUT Val = "<<es_thisGateNum<<":"<<NewVal[es_thisGateNum]<<endl;
			#endif
			UpdateEventList();
			break;
		
		case G_OUTPUT/*2*/:	
			NewVal[es_thisGateNum] = NewVal[faninlist[es_thisGateNum][0]];
			#ifdef DEBUG
				cout<<" OUTPUT Val = "<<es_thisGateNum<<":"<<NewVal[es_thisGateNum]<<endl;
			#endif
			break;
		
		case G_XOR/*3*/	:
			InterResult = NewVal[faninlist[es_thisGateNum][0]];
			#ifdef DEBUG
				cout<<" XOR Input = "<<faninlist[es_thisGateNum][0]<<":"<<InterResult<<" ";
			#endif
			for (unsigned int k=1; k<fanin[es_thisGateNum]; k++)
			{	
				#ifdef DEBUG
					cout<<faninlist[es_thisGateNum][k]<<":"<<NewVal[faninlist[es_thisGateNum][k]]<<" ";
				#endif
				switch(NewVal[faninlist[es_thisGateNum][k]])
				{	
					case -1:switch(InterResult)
					{	case -1:
						case 0:
						case 1: InterResult = -1; break;
						default: printf("InterResult not a valid value in XOR\n"); break;
					}
					break;

					case 0:switch(InterResult)
					{	
						case -1: InterResult = -1; break;
						case 0: InterResult = 0; break;
						case 1: InterResult = 1; break;
						default: printf("InterResult not a valid value in XOR\n"); break;
					}
					break;

					case 1:switch(InterResult)
					{	
						case -1: InterResult = -1; break;
						case 0: InterResult = 1; break;
						case 1: InterResult = 0; break;
						default: printf("InterResult not a valid value in XOR\n"); break;
					}
					break;
					default: printf("Not a valid NewVal[faninlist[es_thisGateNum][k]]\n");
					break;
				}
				if(-1 == InterResult)	break;
			}
			NewVal[es_thisGateNum] = InterResult;
			#ifdef DEBUG		
				cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
			#endif
			UpdateEventList();
			break;
				
		case G_XNOR/*4*/	:
			InterResult = NewVal[faninlist[es_thisGateNum][0]];
			#ifdef DEBUG
				cout<<" XNOR Input = "<<faninlist[es_thisGateNum][0]<<":"<<InterResult<<" ";
			#endif
			for (unsigned int k=1; k<fanin[es_thisGateNum]; k++)
			{
				#ifdef DEBUG
					cout<<faninlist[es_thisGateNum][k]<<":"<<NewVal[faninlist[es_thisGateNum][k]]<<" ";
				#endif
				switch(NewVal[faninlist[es_thisGateNum][k]])
				{	
					case -1:switch(InterResult)
					{	case -1:
						case 0:
						case 1: InterResult = -1; break;
						default: printf("InterResult not a valid value in XOR\n"); break;
					}
					break;

					case 0:switch(InterResult)
					{	
						case -1: InterResult = -1; break;
						case 0: InterResult = 0; break;
						case 1: InterResult = 1; break;
						default: printf("InterResult not a valid value in XOR\n"); break;
					}
					break;

					case 1:switch(InterResult)
					{	
						case -1: InterResult = -1; break;
						case 0: InterResult = 1; break;
						case 1: InterResult = 0; break;
						default: printf("InterResult not a valid value in XOR\n"); break;
					}
					break;
					default: printf("Not a valid NewVal[faninlist[es_thisGateNum][k]]\n");
					break;
				}
				if(-1 == InterResult)	break;
			}
					
			switch(InterResult)
			{
				case -1	:	NewVal[es_thisGateNum] = -1;	break;
				case 1	:	NewVal[es_thisGateNum] = 0;	break;
				case 0	:	NewVal[es_thisGateNum] = 1;	break;
				default: printf("Unknown Input value to NOT gate\n");
			}
			#ifdef DEBUG		
				cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
			#endif
			UpdateEventList();
			break;

		case G_DFF/*5*/	:
			#ifdef DEBUG
				cout<<" DFF Input ="<<faninlist[es_thisGateNum][0]<<":"<<NewVal[faninlist[es_thisGateNum][0]]<<endl;
			#endif
			UpdateEventList();
			//DFF updates in next cycle, so nothing to update now
			break;

		case G_AND/*6*/	:
			InterResult = NewVal[faninlist[es_thisGateNum][0]];
			#ifdef DEBUG
				cout<<" AND Input = "<<faninlist[es_thisGateNum][0]<<":"<<InterResult<<" ";
			#endif
			for (unsigned int k=1; k<fanin[es_thisGateNum]; k++)
			{
				#ifdef DEBUG
					cout<<faninlist[es_thisGateNum][k]<<":"<<NewVal[faninlist[es_thisGateNum][k]]<<" ";
				#endif						
				switch(NewVal[faninlist[es_thisGateNum][k]])
				{	
					case -1:switch(InterResult)
					{	case -1: InterResult = -1; break;
						case 0:	InterResult = 0; break;
						case 1: InterResult = -1; break;
						default: printf("InterResult not a valid value in AND\n"); break;
					}
					break;

					case 0:switch(InterResult)
					{	
						case -1:
						case 0:
						case 1: InterResult = 0; break;
						default: printf("InterResult not a valid value in AND\n"); break;
					}
					break;

					case 1:switch(InterResult)
					{	
						case -1: InterResult = -1; break;
						case 0: InterResult = 0; break;
						case 1: InterResult = 1; break;
						default: printf("InterResult not a valid value in AND\n"); break;
					}
					break;
					default: printf("Not a valid NewVal[faninlist[es_thisGateNum][k]]\n");
					break;
				}
				if(0 == InterResult)	break;
			}
			NewVal[es_thisGateNum] = InterResult;
			#ifdef DEBUG		
				cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
			#endif					
			UpdateEventList();
			break;

		case G_NAND/*7*/	:	
			{	
				#ifdef DEBUG
					cout<<" NAND Input = ";
				#endif
				InterResult=0;
				for (unsigned int k=0;k<fanin[es_thisGateNum];k++)
				{	
					#ifdef DEBUG
						cout<<faninlist[es_thisGateNum][k]<<":"<<NewVal[faninlist[es_thisGateNum][k]]<<" ";
					#endif						
					if(NewVal[faninlist[es_thisGateNum][k]] == 0)
					{	InterResult = 1;
						break;
					}
					else if(NewVal[faninlist[es_thisGateNum][k]] == 1)
					{	if(0 == InterResult)
						{	InterResult = 0;
						}
						else InterResult = -1;
					}
					else
					{	InterResult = -1;
					}
				}
				NewVal[es_thisGateNum] = InterResult;
				#ifdef DEBUG		
					cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
				#endif
				UpdateEventList();
			}
			break;
			
		case G_OR/*8*/		:
			InterResult = NewVal[faninlist[es_thisGateNum][0]];
			#ifdef DEBUG
				cout<<" OR Input = "<<faninlist[es_thisGateNum][0]<<":"<<InterResult<<" ";
			#endif
			for (unsigned int k=1; k<fanin[es_thisGateNum]; k++)
			{	
				#ifdef DEBUG
					cout<<faninlist[es_thisGateNum][k]<<":"<<NewVal[faninlist[es_thisGateNum][k]]<<" ";
				#endif					
				switch(NewVal[faninlist[es_thisGateNum][k]])
				{	
					case -1:switch(InterResult)
					{	case -1: InterResult = -1; break;
						case 0:	InterResult = -1; break;
						case 1: InterResult = 1; break;
						default: printf("InterResult not a valid value in OR\n"); break;
					}
					break;

					case 0:switch(InterResult)
					{	
						case -1: InterResult = -1; break;
						case 0:	InterResult = 0; break;
						case 1: InterResult = 1; break;
						default: printf("InterResult not a valid value in OR\n"); break;
					}
					break;

					case 1:switch(InterResult)
					{	
						case -1:
						case 0:
						case 1: InterResult = 1; break;
						default: printf("InterResult not a valid value in OR\n"); break;
					}
					break;
					default: printf("Not a valid NewVal[faninlist[es_thisGateNum][k]]\n");
					break;
				}
				if(1 == InterResult)	break;
			}
			NewVal[es_thisGateNum] = InterResult;
			#ifdef DEBUG		
				cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
			#endif
			UpdateEventList();
			break;

		case G_NOR/*9*/		:	
			{	
				//printf("Gate Type G_NAND, Gate No %d\n",Sim->thisGateNum);
				#ifdef DEBUG
					cout<<" NOR Input = ";
				#endif
				InterResult=0;
				for (unsigned int k=0;k<fanin[es_thisGateNum];k++)
				{	
					#ifdef DEBUG
						cout<<faninlist[es_thisGateNum][k]<<":"<<NewVal[faninlist[es_thisGateNum][k]]<<" ";
					#endif						
					if(NewVal[faninlist[es_thisGateNum][k]] == 1)
					{	InterResult = 0;
						break;
					}
					else if(NewVal[faninlist[es_thisGateNum][k]] == 0)
					{	if(0 == InterResult)
						{	InterResult = 1;
						}
						else if(1 == InterResult)
						{	InterResult = 1;
						}
						else
						{	InterResult = -1;
						}
					}
					else
					{	InterResult = -1;
					}
				}
				NewVal[es_thisGateNum] = InterResult;
				#ifdef DEBUG		
					cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
				#endif
				UpdateEventList();
			}
			break;

		case G_NOT/*10*/	:
			#ifdef DEBUG
				cout<<" NOT Input = "<<faninlist[es_thisGateNum][0]<<":"<<NewVal[faninlist[es_thisGateNum][0]]<<" ";
			#endif
			switch(NewVal[faninlist[es_thisGateNum][0]])
			{
				case -1	:	NewVal[es_thisGateNum] = -1;	break;
				case 1	:	NewVal[es_thisGateNum] = 0;	break;
				case 0	:	NewVal[es_thisGateNum] = 1;	break;
				default: printf("Unknown Input value to NOT gate\n");
			}
			#ifdef DEBUG		
				cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
			#endif
			UpdateEventList();
			break;
				
		case G_BUF/*11*/	:
			#ifdef DEBUG
				cout<<" BUFFER Input = "<<faninlist[es_thisGateNum][0]<<":"<<NewVal[faninlist[es_thisGateNum][0]]<<" ";
			#endif
			NewVal[es_thisGateNum] = NewVal[faninlist[es_thisGateNum][0]];
			#ifdef DEBUG		
				cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
			#endif	
			UpdateEventList();
			break;
				
		default	: printf("Unknown gate type\n"); exit(22); break;
				
	}
	#ifdef DEBUG
		PrintNew();
	#endif
	return 1;
}

bool circuit::SimFaulty()
{
	
	if(0 == FltVct[FltNo][1]) //Fault is on output
	{	
		#ifdef DEBUG
			cout<<"evaluating "<<es_thisGateNum<<" as output of it is faulty so setting it to "<<FltVct[FltNo][2]<<endl;
		#endif
		
		NewVal[es_thisGateNum] = FltVct[FltNo][2];
		UpdateEventList();
		
		#ifdef DEBUG
			PrintNew();
		#endif
		return 1;
	}
	else
	{	//unsigned int FaultIpNo = FltVct[FltNo][1];
		int InterResult = -1;
		#ifdef DEBUG
			cout<<"evaluating "<<es_thisGateNum;
		#endif

		switch(gtype[es_thisGateNum])
		{
			case G_JUNK/*0*/:	printf("Gate Type: G_JUNK\n");	break;
			
			case G_INPUT/*1*/:	
				#ifdef DEBUG
					cout<<"As Input is faulty so INPUT Val = "<<es_thisGateNum<<":"<<FltVct[FltNo][2]<<endl;
				#endif
				NewVal[es_thisGateNum] = FltVct[FltNo][2];
				UpdateEventList();
				break;
			
			case G_OUTPUT/*2*/:	
				#ifdef DEBUG
					cout<<"As output's input is faulty so OUTPUT Val = "<<es_thisGateNum<<":"<<FltVct[FltNo][2]<<endl;
				#endif
				NewVal[es_thisGateNum] = FltVct[FltNo][2];
				break;

			case G_XOR/*3*/	:	
				InterResult = NewVal[faninlist[es_thisGateNum][0]];
				if(FltVct[FltNo][1] == 1)
				{	InterResult = FltVct[FltNo][2];
					#ifdef DEBUG
						cout<<" XOR 1st input is faulty so Input = "<<faninlist[es_thisGateNum][0]<<":"<<FltVct[FltNo][2]<<" ";
					#endif
				}
				else
				{
					#ifdef DEBUG
						cout<<" XOR Input = "<<faninlist[es_thisGateNum][0]<<":"<<NewVal[faninlist[es_thisGateNum][0]]<<" ";
					#endif
				}

				for (unsigned int k=1; k<fanin[es_thisGateNum]; k++)
				{	
					int Ip2 = NewVal[faninlist[es_thisGateNum][k]];
					if(FltVct[FltNo][1] == k+1)		//if fault is on 2 input it would be in faninlist with index 1
					{	Ip2 = FltVct[FltNo][2];
						#ifdef DEBUG
							cout<<"Input No "<<FltVct[FltNo][1]<<" is faulty so";
							cout<<faninlist[es_thisGateNum][k]<<":"<<Ip2<<" ";
						#endif
					}
					else
					{
						#ifdef DEBUG
							cout<<faninlist[es_thisGateNum][k]<<":"<<NewVal[faninlist[es_thisGateNum][k]]<<" ";
						#endif
					}
						
					switch(Ip2)
					{	
						case -1:switch(InterResult)
						{	case -1:
							case 0:
							case 1: InterResult = -1; break;
							default: printf("InterResult not a valid value in XOR\n"); break;
						}
							break;

						case 0:switch(InterResult)
						{	
							case -1: InterResult = -1; break;
							case 0: InterResult = 0; break;
							case 1: InterResult = 1; break;
							default: printf("InterResult not a valid value in XOR\n"); break;
						}
							break;

						case 1:switch(InterResult)
						{	
							case -1: InterResult = -1; break;
							case 0: InterResult = 1; break;
							case 1: InterResult = 0; break;
							default: printf("InterResult not a valid value in XOR\n"); break;
						}
							break;
						default: printf("Not a valid NewVal[faninlist[es_thisGateNum][k]]\n");
							break;
					}
					if(-1 == InterResult)	break;
				}
				NewVal[es_thisGateNum] = InterResult;
				#ifdef DEBUG		
					cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
				#endif
				UpdateEventList();
				break;

			case G_XNOR/*4*/	:	
				InterResult = NewVal[faninlist[es_thisGateNum][0]];
				if(FltVct[FltNo][1] == 1)
				{	InterResult = FltVct[FltNo][2];
					#ifdef DEBUG
						cout<<" XNOR 1st input is faulty so Input = "<<faninlist[es_thisGateNum][0]<<":"<<FltVct[FltNo][2]<<" ";
					#endif
				}
				else
				{
				#ifdef DEBUG
					cout<<" XNOR Input = "<<faninlist[es_thisGateNum][0]<<":"<<InterResult<<" ";
				#endif
				}

				for (unsigned int k=1; k<fanin[es_thisGateNum]; k++)
				{
					int Ip2 = NewVal[faninlist[es_thisGateNum][k]];
					if(FltVct[FltNo][1] == k+1)		//if fault is on 2 input it would be in faninlist with index 1						
					{	Ip2 = FltVct[FltNo][2];
						#ifdef DEBUG
							cout<<"Input No "<<FltVct[FltNo][1]<<" is faulty so";
							cout<<faninlist[es_thisGateNum][k]<<":"<<Ip2<<" ";
						#endif
					}
					else
					{
						#ifdef DEBUG
							cout<<faninlist[es_thisGateNum][k]<<":"<<NewVal[faninlist[es_thisGateNum][k]]<<" ";
						#endif
					}

					switch(Ip2)
					{	
						case -1:switch(InterResult)
						{	case -1:
							case 0:
							case 1: InterResult = -1; break;
							default: printf("InterResult not a valid value in XOR\n"); break;
						}
							break;

						case 0:switch(InterResult)
						{	
							case -1: InterResult = -1; break;
							case 0: InterResult = 0; break;
							case 1: InterResult = 1; break;
							default: printf("InterResult not a valid value in XOR\n"); break;
						}
							break;

						case 1:switch(InterResult)
						{	
							case -1: InterResult = -1; break;
							case 0: InterResult = 1; break;
							case 1: InterResult = 0; break;
							default: printf("InterResult not a valid value in XOR\n"); break;
						}
							break;
						default: printf("Not a valid NewVal[faninlist[es_thisGateNum][k]]\n");
							break;
					}
					if(-1 == InterResult)	break;
				}
					
				switch(InterResult)
				{
					case -1	:	NewVal[es_thisGateNum] = -1;	break;
					case 1	:	NewVal[es_thisGateNum] = 0;	break;
					case 0	:	NewVal[es_thisGateNum] = 1;	break;
					default: printf("Unknown Input value to NOT gate\n");
				}
				#ifdef DEBUG		
					cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
				#endif
				UpdateEventList();
				break;

			case G_DFF/*5*/	:
				#ifdef DEBUG
					cout<<"Fault on DFF "<<es_thisGateNum<<" Input\n";
					cout<<" DFF Input ="<<faninlist[es_thisGateNum][0]<<":"<<NewVal[faninlist[es_thisGateNum][0]]<<endl;
					cout<<" As it is faulty so would save "<<FltVct[FltNo][2]<<" for next cycle"<<endl;
				#endif
				UpdateEventList();
				//DFF updates in next cycle, so nothing to update now
				break;

			case G_AND/*6*/	:	
				InterResult = NewVal[faninlist[es_thisGateNum][0]];
				if(FltVct[FltNo][1] == 1)
				{	InterResult = FltVct[FltNo][2];
					#ifdef DEBUG
						cout<<" AND 1st input is faulty so Input = "<<faninlist[es_thisGateNum][0]<<":"<<FltVct[FltNo][2]<<" ";
					#endif
				}
				else
				{
					#ifdef DEBUG
						cout<<" AND Input = "<<faninlist[es_thisGateNum][0]<<":"<<InterResult<<" ";
					#endif
				}




				for (unsigned int k=1; k<fanin[es_thisGateNum]; k++)
				{
					int Ip2 = NewVal[faninlist[es_thisGateNum][k]];
					if(FltVct[FltNo][1] == k+1)		//if fault is on 2 input it would be in faninlist with index 1
					{	Ip2 = FltVct[FltNo][2];
						#ifdef DEBUG
							cout<<"Input No "<<FltVct[FltNo][1]<<" is faulty so";
							cout<<faninlist[es_thisGateNum][k]<<":"<<Ip2<<" ";
						#endif
					}
					else
					{
						#ifdef DEBUG
							cout<<faninlist[es_thisGateNum][k]<<":"<<NewVal[faninlist[es_thisGateNum][k]]<<" ";
						#endif
					}
					switch(Ip2)
					{	
						case -1:switch(InterResult)
						{	case -1: InterResult = -1; break;
							case 0:	InterResult = 0; break;
							case 1: InterResult = -1; break;
							default: printf("InterResult not a valid value in AND\n"); break;
						}
							break;

						case 0:switch(InterResult)
						{	
							case -1:
							case 0:
							case 1: InterResult = 0; break;
							default: printf("InterResult not a valid value in AND\n"); break;
						}
							break;

						case 1:switch(InterResult)
						{	
							case -1: InterResult = -1; break;
							case 0: InterResult = 0; break;
							case 1: InterResult = 1; break;
							default: printf("InterResult not a valid value in AND\n"); break;
						}
							break;
						default: printf("Not a valid NewVal[faninlist[es_thisGateNum][k]]\n");
							break;
					}
					if(0 == InterResult)	break;
				}
				NewVal[es_thisGateNum] = InterResult;
				#ifdef DEBUG		
					cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
				#endif					
				UpdateEventList();
				break;

			case G_NAND/*7*/	:	
				{							
					#ifdef DEBUG
						cout<<" NAND Input = ";
					#endif
					InterResult=0;
					for (unsigned int k=0;k<fanin[es_thisGateNum];k++)
					{							
						int Ip2 = NewVal[faninlist[es_thisGateNum][k]];
						if(FltVct[FltNo][1] == k+1)		//if fault is on 2 input it would be in faninlist with index 1
						{	Ip2 = FltVct[FltNo][2];
							#ifdef DEBUG
								cout<<"Input No "<<FltVct[FltNo][1]<<" is faulty so";
								cout<<faninlist[es_thisGateNum][k]<<":"<<Ip2<<" ";
							#endif
						}
						else
						{
							#ifdef DEBUG
								cout<<faninlist[es_thisGateNum][k]<<":"<<NewVal[faninlist[es_thisGateNum][k]]<<" ";
							#endif
						}

						if(Ip2 == 0)
						{	InterResult = 1;
							break;
						}
						else if(Ip2 == 1)
						{	if(0 == InterResult)
								InterResult = 0;
							else InterResult = -1;
						}
						else
						{	InterResult = -1;
						}
					}
					NewVal[es_thisGateNum] = InterResult;
					#ifdef DEBUG		
						cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
					#endif
					UpdateEventList();
				}
				break;

			case G_OR/*8*/		:	
				InterResult = NewVal[faninlist[es_thisGateNum][0]];
				if(FltVct[FltNo][1] == 1)
				{	InterResult = FltVct[FltNo][2];
					#ifdef DEBUG
						cout<<" OR 1st input is faulty so Input = "<<faninlist[es_thisGateNum][0]<<":"<<FltVct[FltNo][2]<<" ";
					#endif
				}
				else
				{
					#ifdef DEBUG
						cout<<" OR Input = "<<faninlist[es_thisGateNum][0]<<":"<<NewVal[faninlist[es_thisGateNum][0]]<<" ";
					#endif
				}


				for (unsigned int k=1; k<fanin[es_thisGateNum]; k++)
				{	
					int Ip2 = NewVal[faninlist[es_thisGateNum][k]];
					if(FltVct[FltNo][1] == k+1)		//if fault is on 2 input it would be in faninlist with index 1
					{	Ip2 = FltVct[FltNo][2];
						#ifdef DEBUG
							cout<<"Input No "<<FltVct[FltNo][1]<<" is faulty so";
							cout<<faninlist[es_thisGateNum][k]<<":"<<Ip2<<" ";
						#endif
					}
					else
					{
						#ifdef DEBUG
							cout<<faninlist[es_thisGateNum][k]<<":"<<NewVal[faninlist[es_thisGateNum][k]]<<" ";
						#endif
					}


					switch(Ip2)
					{	
						case -1:switch(InterResult)
						{	case -1: InterResult = -1; break;
							case 0:	InterResult = -1; break;
							case 1: InterResult = 1; break;
							default: printf("InterResult not a valid value in OR\n"); break;
						}
							break;

						case 0:switch(InterResult)
						{	
							case -1: InterResult = -1; break;
							case 0:	InterResult = 0; break;
							case 1: InterResult = 1; break;
							default: printf("InterResult not a valid value in OR\n"); break;
						}
							break;

						case 1:switch(InterResult)
						{	
							case -1:
							case 0:
							case 1: InterResult = 1; break;
							default: printf("InterResult not a valid value in OR\n"); break;
						}
							break;
						default: printf("Not a valid NewVal[faninlist[es_thisGateNum][k]]\n");
							break;
					}
					if(1 == InterResult)	break;
				}
				NewVal[es_thisGateNum] = InterResult;
				#ifdef DEBUG		
					cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
				#endif
				UpdateEventList();
				break;

			case G_NOR/*9*/		:	
				{	
					#ifdef DEBUG
						cout<<" NOR Input = ";
					#endif
					InterResult=0;
					for (unsigned int k=0;k<fanin[es_thisGateNum];k++)
					{	
						int Ip2 = NewVal[faninlist[es_thisGateNum][k]];
						if(FltVct[FltNo][1] == k+1)		//if fault is on 2 input it would be in faninlist with index 1
						{	Ip2 = FltVct[FltNo][2];
							#ifdef DEBUG
								cout<<"Input No "<<FltVct[FltNo][1]<<" is faulty so";
								cout<<faninlist[es_thisGateNum][k]<<":"<<Ip2<<" ";
							#endif
						}
						else
						{
							#ifdef DEBUG
								cout<<faninlist[es_thisGateNum][k]<<":"<<NewVal[faninlist[es_thisGateNum][k]]<<" ";
							#endif
						}

						if(Ip2 == 1)
						{	InterResult = 0;
							break;
						}
						else if(Ip2 == 0)
						{	if(0 == InterResult)
							{	InterResult = 1;
							}
							else if(1 == InterResult)
							{	InterResult = 1;
							}
							else
							{	InterResult = -1;
							}
						}
						else
						{	InterResult = -1;
						}
					}
					NewVal[es_thisGateNum] = InterResult;
					#ifdef DEBUG		
						cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
					#endif
					UpdateEventList();
				}
				break;

			case G_NOT/*10*/	:
				#ifdef DEBUG
					cout<<" NOT Input is faulty so "<<faninlist[es_thisGateNum][0]<<":"<<FltVct[FltNo][2]<<" ";
				#endif
				switch(FltVct[FltNo][2])
				{
					case -1	:	NewVal[es_thisGateNum] = -1;	break;
					case 1	:	NewVal[es_thisGateNum] = 0;	break;
					case 0	:	NewVal[es_thisGateNum] = 1;	break;
					default: printf("Unknown Input value to NOT gate\n");
				}
				#ifdef DEBUG		
					cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
				#endif	
				UpdateEventList();
				break;

			case G_BUF/*11*/	:
				#ifdef DEBUG
					cout<<" BUFFER Input is faulty so "<<faninlist[es_thisGateNum][0]<<":"<<FltVct[FltNo][2]<<" ";
				#endif
				NewVal[es_thisGateNum] = FltVct[FltNo][2];
				#ifdef DEBUG		
					cout<<"OUTPUT = "<<NewVal[es_thisGateNum]<<endl;
				#endif	
				UpdateEventList();
				break;

			default	: printf("Unknown gate type\n"); exit(22); break;
		}
		#ifdef DEBUG
			PrintNew();
		#endif
	}
	return 1;
}

int main(int argc, char *argv[])
{
char cktName[CKT_NAME_CHARS];
circuit *ckt;

if (argc != 2)
{
	fprintf(stderr, "Usage: %s <ckt>\n", argv[0]);
	exit(-1);
}

strcpy(cktName, argv[1]);

// read in the circuit and build data structure
ckt = new circuit(cktName);
ckt->ReadVecFile(cktName);
ckt->reset(-1);
ckt->DoFaultySim = 0;

for(unsigned int i=0; i<ckt->NumVec; i++)
{	
	ckt->DoFaultySim = 0;
	ckt->reset(-1);
	ckt->ResetEvtQueue();
	fprintf(ckt->Out_Sfile, "vec #%d\n", i);
#ifdef DEBUG2	
	cout<<"vec #"<<i<<endl;
#endif
	ckt->GetNewVect(i);
	ckt->UpdateFFs();	
	
	ckt->PrintInput_outs();
#ifdef DEBUG2
	cout<<"INPUT: ";
	ckt->PrintInput();
#endif
	ckt->SimOneCyc();

	ckt->PrintOutput_outs();
#ifdef DEBUG2
	cout<<"OUTPUT: ";
	ckt->PrintOutput();
#endif
	ckt->SaveFFVal();
	ckt->PrtFFState_outs(NOT_END);
#ifdef DEBUG2
	cout<<"PRESENT STATE: ";
	ckt->PrtFFState();
#endif
	ckt->UpdatePrevNew();
	ckt->GetFltFreeOpVctrs(i);
}
ckt->UpdateFFs();
ckt->PrtFFState_outs(END);

ckt->ResetFFVal();
ckt->FltNo = 0;
ckt->ReadEqfFile(cktName);
ckt->DoFaultySim = 1;

if(1 == ckt->DoFaultySim)
{	bool FaultDetected = 0;
	for(ckt->FltNo = 0; ckt->FltNo<ckt->TotNumFlt; ckt->FltNo++)
	{	FaultDetected = 0;
		ckt->ResetFFVal();
		for(unsigned int i=0; i<ckt->NumVec; i++)
		//with fault
		{	
			#ifdef DEBUG4
				cout<<"With Fault No "<<ckt->FltNo<<" - ";
				cout<<ckt->FltVct[ckt->FltNo][0]<<" "<<ckt->FltVct[ckt->FltNo][1]<<" "<<ckt->FltVct[ckt->FltNo][2]<<endl;
			#endif
			ckt->reset(-1);
			ckt->ResetEvtQueue();
			#ifdef DEBUG4
				cout<<"vec #"<<i<<endl;
			#endif
			ckt->GetNewVect(i);
			ckt->UpdateFFs();
			#ifdef DEBUG4
				cout<<"INPUT: ";
				ckt->PrintInput();
			#endif
			ckt->SimOneCyc();
			#ifdef DEBUG4
				cout<<"OUTPUT: ";
				ckt->PrintOutput();
				cout<<"PRESENT STATE: ";	
			#endif
			ckt->SaveFFVal();
			#ifdef DEBUG4
				ckt->PrtFFState();
			#endif
			
			ckt->UpdatePrevNew();
			if(2 == ckt->CmprFltyOps(i))
			{//	cout<<ckt->FltVct[ckt->FltNo][0]<<" "<<ckt->FltVct[ckt->FltNo][1]<<" "<<ckt->FltVct[ckt->FltNo][2]<<";"<<endl;
			//	cout<<" detected by vector "<<i<<endl;
				fprintf(ckt->Det_Sfile, "%d %d %d;\n", ckt->FltVct[ckt->FltNo][0], ckt->FltVct[ckt->FltNo][1], ckt->FltVct[ckt->FltNo][2]);
				FaultDetected = 1;
				break;
			}
		}
		if(0 == FaultDetected)
		{
			fprintf(ckt->Ufl_Sfile, "%d %d %d;\n", ckt->FltVct[ckt->FltNo][0], ckt->FltVct[ckt->FltNo][1], ckt->FltVct[ckt->FltNo][2]);
		}
	}
}

fflush(ckt->Out_Sfile);		fclose(ckt->Out_Sfile);
fflush(ckt->Ufl_Sfile);		fclose(ckt->Ufl_Sfile);
fflush(ckt->Det_Sfile);		fclose(ckt->Det_Sfile);
free(ckt);
return 11;
}
