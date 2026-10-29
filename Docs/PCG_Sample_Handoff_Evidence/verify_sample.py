import unreal,json,os,math,hashlib,collections
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
try:
 game=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
 if game:world=game
except Exception:pass
allactors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
host=next(a for a in allactors if a.get_class().get_name()=='CityPackingActor')
floor=host.get_editor_property('floor')
fo,fe=floor.get_actor_bounds(False)
gap=host.get_editor_property('building_gap'); margin=host.get_editor_property('edge_margin')
records=[]
for a in allactors:
 if not a.actor_has_tag('PCG_SampleBuilding'):continue
 o,e=a.get_actor_bounds(False,True); p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d()
 records.append(dict(name=a.get_name(),kind=a.get_class().get_name(),lo=[o.x-e.x,o.y-e.y],hi=[o.x+e.x,o.y+e.y],location=[p.x,p.y,p.z],yaw=r.yaw,scale=[s.x,s.y,s.z]))
overlaps=[];violations=[];outside=[];nearest=[]
for i,a in enumerate(records):
 if a['lo'][0]<fo.x-fe.x+margin-1 or a['lo'][1]<fo.y-fe.y+margin-1 or a['hi'][0]>fo.x+fe.x-margin+1 or a['hi'][1]>fo.y+fe.y-margin+1:outside.append(a['name'])
 distances=[]
 for j,b in enumerate(records):
  if i==j:continue
  dx=max(a['lo'][0]-b['hi'][0],b['lo'][0]-a['hi'][0]);dy=max(a['lo'][1]-b['hi'][1],b['lo'][1]-a['hi'][1])
  distances.append(math.hypot(max(0,dx),max(0,dy)))
  if j>i:
   if dx<-.1 and dy<-.1:overlaps.append([a['name'],b['name']])
   if max(dx,dy)<gap-1:violations.append(dict(a=a['name'],b=b['name'],dx=dx,dy=dy))
 if distances:nearest.append(min(distances))
signature=hashlib.sha256(json.dumps(sorted((a['kind'],[round(x,2) for x in a['location']],round(a['yaw'],2)) for a in records)).encode()).hexdigest()
result=dict(world=world.get_path_name(),count=len(records),expected=host.get_editor_property('last_building_count'),seed=host.get_editor_property('last_layout_seed'),gap=gap,coverage=host.get_editor_property('last_coverage_percent'),kinds=dict(collections.Counter(a['kind'] for a in records)),yaws=sorted(set(round(a['yaw'])%360 for a in records)),nearest_min=min(nearest) if nearest else None,nearest_max=max(nearest) if nearest else None,overlaps=overlaps,clearance_violations=violations,outside=outside,signature=signature,actors=records)
result['passed']=len(records)==result['expected'] and len(records)>0 and not overlaps and not violations and not outside and all(all(abs(s-1)<1e-5 for s in a['scale']) for a in records)
with open(os.path.join(_city_dir,'sample_verification.json'),'w') as f:json.dump(result,f,indent=2)
