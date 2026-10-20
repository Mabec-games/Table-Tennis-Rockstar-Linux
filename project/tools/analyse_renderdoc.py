"""Run through qrenderdoc --python; exports private replay evidence, then exits.

No game execution, capture edits, GUI interaction or network access. A Vulkan
replay device is required. Outputs stay under ignored build/linux/.
"""
from pathlib import Path
import json
import os
import traceback
import renderdoc as rd

# qrenderdoc startup scripts do not define __file__. The shell wrapper sets this.
ROOT = Path(os.environ.get('TT_PROJECT_ROOT', str(Path.cwd()))).resolve()
OUT = ROOT / 'build/linux/renderdoc-analysis'
CAPTURE = ROOT / 'build/linux/renderdoc-captures/guest-frames_capture.rdc'
OUT.mkdir(parents=True, exist_ok=True)
LOG = (OUT / 'progress.log').open('w', buffering=1)
for stale in ('error.txt', 'summary.json'):
    (OUT / stale).unlink(missing_ok=True)


def log(message):
    LOG.write(str(message) + '\n')


def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2, allow_nan=True))


def rid(value):
    return int(value)


def flatten(actions):
    for action in actions:
        yield action
        yield from flatten(action.children)


def colour(value):
    return [float(v) for v in value.col.floatValue]


def main():
    cap = rd.OpenCaptureFile()
    controller = None
    try:
        result = cap.OpenFile(str(CAPTURE), '', None)
        if result != rd.ResultCode.Succeeded:
            raise RuntimeError('OpenFile: ' + str(result))
        log('Opening Vulkan replay; this may take a few minutes.')
        result, controller = cap.OpenCapture(rd.ReplayOptions(), None)
        if result != rd.ResultCode.Succeeded:
            raise RuntimeError('OpenCapture: ' + str(result))
        actions = list(flatten(controller.GetRootActions()))
        action_map = {a.eventId: a for a in actions}
        resources = {r.resourceId: r.name for r in controller.GetResources()}
        textures = {t.resourceId: t for t in controller.GetTextures()}
        draws = [a for a in actions if a.flags & rd.ActionFlags.Drawcall]
        write('actions.json', [dict(event=a.eventId, flags=str(a.flags),
              count=a.numIndices, instances=a.numInstances,
              outputs=[rid(r) for r in a.outputs if r != rd.ResourceId.Null()],
              depth=rid(a.depthOut), source=rid(a.copySource), destination=rid(a.copyDestination))
              for a in actions])
        write('textures.json', [dict(id=rid(t.resourceId), name=resources.get(t.resourceId,''),
              width=t.width, height=t.height, samples=t.msSamp, format=t.format.Name())
              for t in textures.values()])
        write('debug-messages.json', [dict(event=m.eventId, description=m.description,
              severity=str(m.severity)) for m in controller.GetDebugMessages()])
        log('Indexed {} actions, {} draws, {} textures'.format(len(actions),len(draws),len(textures)))
        dumped_shaders = set()
        dumped_events = set()
        event_errors = []

        def save_texture(resource, name):
            save = rd.TextureSave()
            save.resourceId = resource
            save.destType = rd.FileType.PNG
            save.alpha = rd.AlphaMapping.Discard
            save.mip = 0
            save.slice.sliceIndex = 0
            save.sample.sampleIndex = 0
            result = controller.SaveTexture(save, str(OUT / name))
            log('Saved {}: {}'.format(name,result))

        def dump_event(event):
            if event in dumped_events:
                return
            dumped_events.add(event)
            controller.SetFrameEvent(event, True)
            pipe = controller.GetPipelineState()
            viewport=pipe.GetViewport(0)
            details = dict(event=event, graphics_pipeline=rid(pipe.GetGraphicsPipelineObject()),
                           viewport={key: getattr(viewport,key) for key in ('x','y','width','height','minDepth','maxDepth')},
                           outputs=[rid(d.resource) for d in pipe.GetOutputTargets()], stages={})
            for stage in (rd.ShaderStage.Vertex, rd.ShaderStage.Pixel, rd.ShaderStage.Compute):
                reflection = pipe.GetShaderReflection(stage)
                if reflection is None:
                    continue
                stage_name = {rd.ShaderStage.Vertex: 'vertex', rd.ShaderStage.Pixel: 'fragment',
                              rd.ShaderStage.Compute: 'compute'}[stage]
                shader_id = rid(pipe.GetShader(stage))
                stage_data = dict(shader=shader_id, inputs=[], constants=[])
                details['stages'][stage_name] = stage_data
                if shader_id not in dumped_shaders:
                    dumped_shaders.add(shader_id)
                    (OUT / ('shader-{}.spv'.format(shader_id))).write_bytes(bytes(reflection.rawBytes))
                for used in pipe.GetReadOnlyResources(stage):
                    desc = used.descriptor
                    resource = desc.resource
                    tex = textures.get(resource)
                    item = dict(binding_index=used.access.index, resource=rid(resource),
                                name=resources.get(resource,''), offset=desc.byteOffset,
                                size=desc.byteSize, swizzle=[str(getattr(desc.swizzle, channel))
                                         for channel in ('red', 'green', 'blue', 'alpha')])
                    stage_data['inputs'].append(item)
                    if tex is not None and tex.width <= 4096 and tex.height <= 4096:
                        item.update(width=tex.width,height=tex.height,format=tex.format.Name())
                        filename='event-{}-{}-binding-{}-texture-{}.png'.format(event,stage_name,used.access.index,rid(resource))
                        try:
                            save_texture(resource,filename)
                        except Exception as exc:
                            item['export_error']=str(exc)
                for index, block in enumerate(reflection.constantBlocks):
                    used = pipe.GetConstantBlock(stage,index,0)
                    desc = used.descriptor
                    item = dict(index=index,name=block.name,resource=rid(desc.resource),
                                offset=desc.byteOffset,size=desc.byteSize)
                    stage_data['constants'].append(item)
                    # Save just the bound block, never the whole shared memory allocation.
                    if desc.resource != rd.ResourceId.Null() and 0 < desc.byteSize <= 65536:
                        filename='event-{}-{}-constant-{}.bin'.format(event,stage_name,index)
                        (OUT / filename).write_bytes(bytes(controller.GetBufferData(desc.resource,desc.byteOffset,desc.byteSize)))
                        item['file']=filename
            write('event-{}.json'.format(event),details)
            (OUT / ('event-{}-error.txt'.format(event))).unlink(missing_ok=True)

        # Capture the final source image and its output. Then query likely scene
        # render targets independently: transfers through guest RAM break the
        # texture-only dependency chain.
        if draws:
            controller.SetFrameEvent(draws[-1].eventId,True)
            pipe=controller.GetPipelineState()
            for used in pipe.GetReadOnlyResources(rd.ShaderStage.Pixel):
                if used.descriptor.resource in textures:
                    save_texture(used.descriptor.resource,'final-input-{}.png'.format(rid(used.descriptor.resource)))
        scores={}
        for action in draws:
            for resource in action.outputs:
                tex=textures.get(resource)
                if tex is None or tex.width < 320 or tex.height < 256:
                    continue
                scores[resource]=scores.get(resource,0)+(10 if action.numIndices>6 else 1)
        targets=sorted(scores,key=lambda r:(rid(r) in (821,12223),scores[r]),reverse=True)[:8]
        write('history-targets.json',[dict(resource=rid(r),score=scores[r]) for r in targets])
        for resource in targets:
            tex=textures[resource]
            # Face location chosen in the captured final image (910,377 at 2560x1440).
            # Tall EDRAM textures contain the 720-line game image at the top.
            scale=tex.width/2560.0
            x,y=round(910*scale),round(377*scale)
            log('Pixel history texture {} ({},{}), {}x{}'.format(rid(resource),x,y,tex.width,tex.height))
            controller.SetFrameEvent(actions[-1].eventId,True)
            save_texture(resource, 'target-{}.png'.format(rid(resource)))
            history=controller.PixelHistory(resource,x,y,rd.Subresource(),rd.CompType.Typeless)
            rows=[]
            for mod in history:
                rows.append(dict(event=mod.eventId,passed=mod.Passed(),fragment=mod.fragIndex,
                                 before=colour(mod.preMod),after=colour(mod.postMod),shader=colour(mod.shaderOut),
                                 depth_failed=mod.depthTestFailed,discarded=mod.shaderDiscarded,
                                 direct_write=mod.directShaderWrite))
            write('history-{}.json'.format(rid(resource)),rows)
            log('{} history entries'.format(len(rows)))
            candidates=[]
            for row in rows:
                action=action_map.get(row['event'])
                if row['passed'] and action is not None and action.flags & rd.ActionFlags.Drawcall:
                    if row['event'] not in candidates:
                        candidates.append(row['event'])
            # Include last writes plus the first substantial geometry writes.
            substantial=[e for e in candidates if action_map[e].numIndices>6]
            selected=list(dict.fromkeys(substantial[-8:]+candidates[-4:]))
            for event in selected:
                log('Exporting event {}'.format(event))
                try:
                    dump_event(event)
                except Exception:
                    event_errors.append(event)
                    (OUT / ('event-{}-error.txt'.format(event))).write_text(traceback.format_exc())
        write('summary.json',dict(status='partial' if event_errors else 'complete',failed_events=event_errors,capture=str(CAPTURE),actions=len(actions),
              draws=len(draws),targets=[rid(r) for r in targets],exported_events=sorted(dumped_events)))
        log(('PARTIAL: event export errors. ' if event_errors else 'DONE. ') + 'Evidence written to '+str(OUT))
    finally:
        if controller is not None:
            controller.Shutdown()
        cap.Shutdown()


if __name__ == '__main__':
    exit_code=0
    try:
        main()
    except BaseException:
        exit_code=1
        (OUT/'error.txt').write_text(traceback.format_exc())
        log(traceback.format_exc())
    LOG.close()
    # qrenderdoc otherwise opens its main UI after the startup script returns.
    os._exit(exit_code)
