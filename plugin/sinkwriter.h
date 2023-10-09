#pragma once

#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <d3d11.h>

void Hook();
void Unhook();

STDAPI SinkWriterSetInputMediaType(IMFSinkWriter* pThis, DWORD dwStreamIndex, IMFMediaType* pInputMediaType, IMFAttributes* pEncodingParameters);
STDAPI SinkWriterBeginWriting(IMFSinkWriter* pThis);
STDAPI SinkWriterWriteSample(IMFSinkWriter* pThis, DWORD dwStreamIndex, IMFSample* pSample);
STDAPI SinkWriterFlush(IMFSinkWriter* pThis, DWORD dwStreamIndex);
STDAPI SinkWriterFinalize(IMFSinkWriter* pThis);
STDAPI CreateSinkWriterFromURL(LPCWSTR pwszOutputURL, IMFByteStream* pByteStream, IMFAttributes* pAttributes, IMFSinkWriter** ppSinkWriter);
void STDMETHODCALLTYPE DeviceContextOMSetRenderTargets(
	ID3D11DeviceContext* pThis,
	UINT NumViews,
	ID3D11RenderTargetView* const* ppRenderTargetViews,
	ID3D11DepthStencilView* pDepthStencilView);
