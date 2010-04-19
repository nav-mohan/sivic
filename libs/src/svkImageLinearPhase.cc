/*
 *  Copyright © 2009-2010 The Regents of the University of California.
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions are met:
 *  •   Redistributions of source code must retain the above copyright notice, 
 *      this list of conditions and the following disclaimer.
 *  •   Redistributions in binary form must reproduce the above copyright notice, 
 *      this list of conditions and the following disclaimer in the documentation 
 *      and/or other materials provided with the distribution.
 *  •   None of the names of any campus of the University of California, the name 
 *      "The Regents of the University of California," or the names of any of its 
 *      contributors may be used to endorse or promote products derived from this 
 *      software without specific prior written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 *  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
 *  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 *  OF SUCH DAMAGE.
 */ 
/*
 *  $URL: https://sivic.svn.sourceforge.net/svnroot/sivic/trunk/libs/src/svkMrsImageFFT.cc $
 *  $Rev: 259 $
 *  $Author: beckn8tor $
 *  $Date: 2010-04-12 16:51:07 -0700 (Mon, 12 Apr 2010) $
 *
 *  Authors:
 *      Jason C. Crane, Ph.D.
 *      Beck Olson
 */

/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkImageRFFT.cxx,v $

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include <svkImageLinearPhase.h>

using namespace svk;

vtkCxxRevisionMacro(svkImageLinearPhase, "$Revision: 1.37 $");
vtkStandardNewMacro(svkImageLinearPhase);


/*!
 *
 */
svkImageLinearPhase::svkImageLinearPhase() 
{
    this->SetNumberOfThreads(1);
    this->shiftWindow = 0;
}


/*!
 *
 */
svkImageLinearPhase::~svkImageLinearPhase()
{

}


/*!
 *
 */
void svkImageLinearPhase::SetShiftWindow( double shiftWindow )
{
    this->shiftWindow = shiftWindow;
}


void vtkImageLinearPhaseInternalRequestUpdateExtent(int *inExt, int *outExt, 
                                             int *wExt,
                                             int iteration)
{
  memcpy(inExt, outExt, 6 * sizeof(int));
  inExt[iteration*2] = wExt[iteration*2];
  inExt[iteration*2 + 1] = wExt[iteration*2 + 1];  
}

//----------------------------------------------------------------------------
// This templated execute method handles any type input, but the output
// is always doubles.
template <class T>
void vtkImageLinearPhaseExecute(svkImageLinearPhase *self,
                         vtkImageData *inData, int inExt[6], T *inPtr,
                         vtkImageData *outData, int outExt[6], double *outPtr,
                         int id)
{
  vtkImageComplex *inComplex;
  vtkImageComplex *outComplex;
  vtkImageComplex *pComplex;
  //
  int inMin0, inMax0;
  vtkIdType inInc0, inInc1, inInc2;
  T *inPtr0, *inPtr1, *inPtr2;
  //
  int outMin0, outMax0, outMin1, outMax1, outMin2, outMax2;
  vtkIdType outInc0, outInc1, outInc2;
  double *outPtr0, *outPtr1, *outPtr2;
  //
  int idx0, idx1, idx2, inSize0, numberOfComponents;
  unsigned long count = 0;
  unsigned long target;
  double startProgress;

  startProgress = self->GetIteration()/
    static_cast<double>(self->GetNumberOfIterations());
  
  // Reorder axes (The outs here are just placeholdes
  self->PermuteExtent(inExt, inMin0, inMax0, outMin1,outMax1,outMin2,outMax2);
  self->PermuteExtent(outExt, outMin0,outMax0,outMin1,outMax1,outMin2,outMax2);
  self->PermuteIncrements(inData->GetIncrements(), inInc0, inInc1, inInc2);
  self->PermuteIncrements(outData->GetIncrements(), outInc0, outInc1, outInc2);

  
  inSize0 = inMax0 - inMin0 + 1;
  
  // Input has to have real components at least.
  numberOfComponents = inData->GetNumberOfScalarComponents();
  if (numberOfComponents < 1)
    {
    vtkGenericWarningMacro("No real components");
    return;
    }

  // Allocate the arrays of complex numbers
  inComplex = new vtkImageComplex[inSize0];
  outComplex = new vtkImageComplex[inSize0];

  target = static_cast<unsigned long>((outMax2-outMin2+1)*(outMax1-outMin1+1)
                                      * self->GetNumberOfIterations() / 50.0);
  target++;

  // loop over other axes
  inPtr2 = inPtr;
  outPtr2 = outPtr;
  for (idx2 = outMin2; idx2 <= outMax2; ++idx2)
    {
    inPtr1 = inPtr2;
    outPtr1 = outPtr2;
    for (idx1 = outMin1; !self->AbortExecute && idx1 <= outMax1; ++idx1)
      {
      if (!id) 
        {
        if (!(count%target))
          {
          self->UpdateProgress(count/(50.0*target) + startProgress);
          }
        count++;
        }
      // copy into complex numbers
      inPtr0 = inPtr1;
      pComplex = inComplex;
      for (idx0 = inMin0; idx0 <= inMax0; ++idx0)
        {
        pComplex->Real = static_cast<double>(*inPtr0);
        pComplex->Imag = 0.0;
        if (numberOfComponents > 1)
          { // yes we have an imaginary input
          pComplex->Imag = static_cast<double>(inPtr0[1]);;
          }
        inPtr0 += inInc0;
        ++pComplex;
        }
     
      // Call the method that performs the RFFT
        self->ExecuteLinearPhase(inComplex, outComplex, inSize0);

      // copy into output
      outPtr0 = outPtr1;
      pComplex = outComplex + (outMin0 - inMin0);
      for (idx0 = outMin0; idx0 <= outMax0; ++idx0)
        {
        *outPtr0 = static_cast<double>(pComplex->Real);
        outPtr0[1] = static_cast<double>(pComplex->Imag);
        outPtr0 += outInc0;
        ++pComplex;
        }
      inPtr1 += inInc1;
      outPtr1 += outInc1;
      }
    inPtr2 += inInc2;
    outPtr2 += outInc2;
    }
    
  delete [] inComplex;
  delete [] outComplex;
}




//----------------------------------------------------------------------------
// This method is passed input and output Datas, and executes the RFFT
// algorithm to fill the output from the input.
// Not threaded yet.
void svkImageLinearPhase::ThreadedExecute(vtkImageData *inData, vtkImageData *outData,
                                  int outExt[6], int threadId)
{
  void *inPtr, *outPtr;
  int inExt[6];

  int *wExt = inData->GetWholeExtent();
  vtkImageLinearPhaseInternalRequestUpdateExtent(inExt,outExt,wExt,this->Iteration);
  inPtr = inData->GetScalarPointerForExtent(inExt);
  outPtr = outData->GetScalarPointerForExtent(outExt);
  
  // this filter expects that the output be doubles.
  if (outData->GetScalarType() != VTK_DOUBLE)
    {
    vtkErrorMacro(<< "Execute: Output must be be type double.");
    return;
    }

  // this filter expects input to have 1 or two components
  if (outData->GetNumberOfScalarComponents() != 1 && 
      outData->GetNumberOfScalarComponents() != 2)
    {
    vtkErrorMacro(<< "Execute: Cannot handle more than 2 components");
    return;
    }

  // choose which templated function to call.
  switch (inData->GetScalarType())
    {
    vtkTemplateMacro(
      vtkImageLinearPhaseExecute(this, inData, inExt, 
                          static_cast<VTK_TT *>(inPtr), outData, outExt, 
                          static_cast<double *>(outPtr), threadId));
    default:
      vtkErrorMacro(<< "Execute: Unknown ScalarType");
      return;
    }
}


void svkImageLinearPhase::ExecuteLinearPhase( vtkImageComplex* in, vtkImageComplex* out, int N )
{
    //int origin = N/2 + 1;
    int origin = 0;
    double phaseIncrement;
    double oldReal;
    double newReal;
    double oldImag;
    double newImag;
    double phaseReal;
    double phaseImag;
    double mult;
    double shift = -0.5;
    for( int i=0; i < N; i++ ) {
        phaseIncrement = (i - origin)/((double)(N));
        mult = -2 * vtkMath::Pi() * phaseIncrement * shift;
        oldReal = in[i].Real;
        oldImag = in[i].Imag;
        
        //phase = exp( j * mult);
        phaseReal = cos(mult);
        phaseImag = sin(mult);

        // complex multiplication: (x + yi)(u + vi) = (xu – yv) + (xv + yu)i
        newReal = ( phaseReal*oldReal - phaseImag*oldImag );
        newImag = ( phaseReal*oldImag + phaseImag*oldReal );

        out[i].Real = newReal;
        out[i].Imag = newImag;

    }

}