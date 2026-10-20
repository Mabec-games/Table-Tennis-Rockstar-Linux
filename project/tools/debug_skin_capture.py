"""Export two pixel shader traces from the saved private capture, without UI."""
import json
import os
from pathlib import Path
import traceback
import sys
import renderdoc as rd

ROOT=Path(os.environ['TT_PROJECT_ROOT'])
sys.path.insert(0,str(ROOT/'project/tools'))
from spirv_debug_compat import without_explicit_rte
OUT=ROOT/'build/linux/renderdoc-analysis/skin-debug'
OUT.mkdir(parents=True,exist_ok=True)
LOG=(OUT/'progress.log').open('w',buffering=1)
(OUT/'error.txt').unlink(missing_ok=True)


def variable(v,depth=0):
    size=min(16,max(1,v.rows*v.columns))
    result=dict(name=v.name,type=str(v.type),rows=v.rows,columns=v.columns,
                floats=[float(x) for x in v.value.f32v[:size]],
                uints=[int(x) for x in v.value.u32v[:size]])
    if depth<5:
        result['members']=[variable(m,depth+1) for m in v.members]
    return result


def main():
    cap=rd.OpenCaptureFile()
    controller=None
    replacement=None
    original_shader=None
    try:
        result=cap.OpenFile(str(ROOT/'build/linux/renderdoc-captures/guest-frames_capture.rdc'),'',None)
        if result!=rd.ResultCode.Succeeded:
            raise RuntimeError(str(result))
        LOG.write('Opening saved capture for shader debugging\n')
        result,controller=cap.OpenCapture(rd.ReplayOptions(),None)
        if result!=rd.ResultCode.Succeeded:
            raise RuntimeError(str(result))
        for event in (33593,34482):
            LOG.write('Debugging skin shader at event {} pixel (455,188), sample 0\n'.format(event))
            controller.SetFrameEvent(event,True)
            pipe=controller.GetPipelineState()
            reflection=pipe.GetShaderReflection(rd.ShaderStage.Pixel)
            if reflection is None:
                raise RuntimeError('No fragment shader bound')
            if not reflection.debugInfo.debuggable:
                reason=reflection.debugInfo.debugStatus
                LOG.write('Original shader debugger status: '+reason+'\n')
                if 'RoundingModeRTE' not in reason:
                    raise RuntimeError('Unsupported debugger feature: '+reason)
                original_shader=pipe.GetShader(rd.ShaderStage.Pixel)
                target=pipe.GetOutputTargets()[0].resource
                sub=rd.Subresource()
                sub.sample=0
                before=list(controller.PickPixel(target,455,188,sub,rd.CompType.Typeless).floatValue)
                modified=without_explicit_rte(bytes(reflection.rawBytes))
                replacement,errors=controller.BuildTargetShader('main',rd.ShaderEncoding.SPIRV,
                    modified,rd.ShaderCompileFlags(),rd.ShaderStage.Pixel)
                if replacement==rd.ResourceId.Null():
                    raise RuntimeError('Replay shader build failed: '+errors)
                controller.ReplaceResource(original_shader,replacement)
                controller.SetFrameEvent(event,True)
                after=list(controller.PickPixel(target,455,188,sub,rd.CompType.Typeless).floatValue)
                comparison=dict(event=event,original=before,replay_copy=after,
                                note='Replay-only omission of explicit RTE declarations; original capture unchanged')
                (OUT/f'event-{event}-compatibility.json').write_text(json.dumps(comparison,indent=2))
                if any(abs(a-b)>1e-6 or not (a==a and b==b) for a,b in zip(before,after)):
                    raise RuntimeError('Replay-only workaround changed the sampled pixel; stopped')
                LOG.write('Replay-only compatibility copy matches original sampled pixel\n')
            options=rd.DebugPixelInputs()
            options.sample=0
            trace=controller.DebugPixel(455,188,options)
            if trace is None:
                raise RuntimeError('No debug trace at event '+str(event))
            try:
                if trace.debugger is None:
                    raise RuntimeError('Shader debugger unavailable at event '+str(event))
                metadata=dict(event=event,inputs=[variable(v) for v in trace.inputs],
                              constants=[variable(v) for v in trace.constantBlocks])
                (OUT/f'event-{event}-inputs.json').write_text(json.dumps(metadata,indent=2))
                count=0
                with (OUT/f'event-{event}-steps.jsonl').open('w') as stream:
                    for batch in range(1000):
                        states=controller.ContinueDebug(trace.debugger)
                        if not states:
                            break
                        for state in states:
                            stream.write(json.dumps(dict(step=state.stepIndex,
                                instruction=state.nextInstruction,flags=str(state.flags),
                                changes=[dict(before=variable(c.before),after=variable(c.after))
                                         for c in state.changes]))+'\n')
                        count+=len(states)
                    else:
                        raise RuntimeError('Debugger exceeded 1000 batches; stopped')
                LOG.write('Exported {} shader steps for event {}\n'.format(count,event))
            finally:
                controller.FreeTrace(trace)
                if replacement is not None:
                    controller.RemoveReplacement(original_shader)
                    controller.FreeTargetResource(replacement)
                    replacement=None
        LOG.write('DONE. Skin shader traces exported.\n')
    finally:
        if controller is not None:
            if replacement is not None:
                controller.RemoveReplacement(original_shader)
                controller.FreeTargetResource(replacement)
            controller.Shutdown()
        cap.Shutdown()


try:
    main()
    code=0
except BaseException:
    (OUT/'error.txt').write_text(traceback.format_exc())
    LOG.write(traceback.format_exc())
    code=1
LOG.close()
os._exit(code)
