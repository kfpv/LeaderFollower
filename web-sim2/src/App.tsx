import { useState, useEffect, useRef } from 'react'
import LEDVisualization from './components/LEDVisualization'
import ControlPanel from './components/ControlPanel'
import './App.css'

interface WASMModule {
  _anim_branches: () => number
  _anim_leds_per_branch: () => number
  _anim_total_leds: () => number
  _anim_eval2: (animId: number, t: number, a: number, b: number, c: number, d: number) => void
  _anim_value_at: (index: number) => number
  // New dynamic parameter functions
  _anim_count: () => number
  _anim_name: (index: number) => number
  _anim_param_count: (animIndex: number) => number
  _anim_param_id: (animIndex: number, paramIndex: number) => number
  _param_info: (paramId: number, nameOut: number, minOut: number, maxOut: number, defOut: number, typeOut: number) => void
  _param_set: (paramId: number, value: number) => void
  _param_get: (paramId: number) => number
  _anim_eval2_global: (animId: number, t: number) => void
  _anim_get_value: (index: number) => number
  // Safe string handling functions
  _anim_name_safe: (index: number, buffer: number, bufferSize: number) => void
  _param_name_safe: (paramId: number, buffer: number, bufferSize: number) => void
  // Memory management functions
  _malloc: (size: number) => number
  _free: (ptr: number) => void
  // Memory reading helper functions
  _read_uint8: (addr: number) => number
  _read_int8: (addr: number) => number
  _read_uint16: (addr: number) => number
  _read_int16: (addr: number) => number
  _read_uint32: (addr: number) => number
  _read_int32: (addr: number) => number
  _read_float: (addr: number) => number
  _read_double: (addr: number) => number
}



interface GlobalState {
  globalSpeed: number
}

interface ParameterInfo {
  id: number
  name: string
  type: 'bool' | 'range' | 'int' | 'enum'
  min: number
  max: number
  default: number
}

interface AnimationInfo {
  index: number
  name: string
  parameters: ParameterInfo[]
}



const App = () => {
  const [module, setModule] = useState<WASMModule | null>(null)
  const [isPaused, setIsPaused] = useState(false)
  const [branches, setBranches] = useState(4)
  const [ledsPerBranch, setLedsPerBranch] = useState(7)
  const [totalLeds, setTotalLeds] = useState(28)
  const [animations, setAnimations] = useState<AnimationInfo[]>([])
  
  // Animation states for left and right trees
  const [leftAnimation, setLeftAnimation] = useState(1) // wave
  const [rightAnimation, setRightAnimation] = useState(1) // wave
  const [leftValues, setLeftValues] = useState<Float32Array>(new Float32Array(28))
  const [rightValues, setRightValues] = useState<Float32Array>(new Float32Array(28))
  
  const [globalControls, setGlobalControls] = useState<GlobalState>({
    globalSpeed: 1.0
  })
  
  // Store separate parameter values for left and right trees (initialized dynamically)
  const [leftParams, setLeftParams] = useState<{[key: number]: number}>({})
  const [rightParams, setRightParams] = useState<{[key: number]: number}>({})
  
  const animationRef = useRef<number | null>(null)
  const startTimeRef = useRef<number>(Date.now())

  // ----- Import/Export helpers -----
  type SideConfig = { animIndex: number, params: Array<{id:number,value:number}> }
  type ExportConfig = { v:1, globals:{ globalSpeed:number }, leader: SideConfig, follower: SideConfig }

  const currentAnimParamIds = (animIndex: number): number[] => {
    const anim = animations.find(a => a.index === animIndex)
    return anim ? anim.parameters.map(p => p.id) : []
  }

  const buildSideConfig = (animIndex: number, side: 'left'|'right'): SideConfig => {
    const ids = currentAnimParamIds(animIndex)
    const params = ids.map(id => ({ id, value: getParameterValue(id, side) }))
    return { animIndex, params }
  }

  const buildExportConfig = (): ExportConfig => {
    return {
      v: 1,
      globals: { globalSpeed: globalControls.globalSpeed ?? 1.0 },
      leader: buildSideConfig(leftAnimation, 'left'),
      follower: buildSideConfig(rightAnimation, 'right')
    }
  }

  const validateConfig = (cfg: any): string|null => {
    if (!cfg || typeof cfg !== 'object') return 'not an object'
    if (cfg.v !== 1) return 'unsupported version'
    if (!cfg.leader || !cfg.follower) return 'missing leader/follower'
    const checkSide = (s:any) => typeof s.animIndex==='number' && Array.isArray(s.params) && s.params.every((p:any)=>p && typeof p.id==='number' && typeof p.value==='number')
    if (!checkSide(cfg.leader) || !checkSide(cfg.follower)) return 'bad side payload'
    if (cfg.globals && typeof cfg.globals.globalSpeed !== 'number') return 'bad globals'
    return null
  }

  const copyToClipboard = async (text:string): Promise<boolean> => {
    try { await navigator.clipboard.writeText(text); return true } catch { return false }
  }
  const readFromClipboard = async (): Promise<string|null> => {
    try { return await navigator.clipboard.readText() } catch { return null }
  }

  const applyImportedConfig = (cfg: ExportConfig) => {
    // globals
    if (cfg.globals && typeof cfg.globals.globalSpeed === 'number') {
      setGlobalControls(prev => ({ ...prev, globalSpeed: cfg.globals.globalSpeed }))
      if (module) module._param_set(20, cfg.globals.globalSpeed)
    }
    // leader/follower selections
    setLeftAnimation(cfg.leader.animIndex)
    setRightAnimation(cfg.follower.animIndex)
    // update parameter maps for ids provided
    const toMap = (base: {[k:number]:number}, side: SideConfig) => {
      const next = { ...base }
      side.params.forEach(p => { next[p.id] = p.value })
      return next
    }
    setLeftParams(prev => toMap(prev, cfg.leader))
    setRightParams(prev => toMap(prev, cfg.follower))
  }

  const doExport = async () => {
    const cfg = buildExportConfig()
    const text = JSON.stringify(cfg)
    const ok = await copyToClipboard(text)
    if (!ok) {
      window.prompt('Copy config JSON:', text)
    }
  }

  const doImport = async () => {
    let text = await readFromClipboard()
    if (!text) text = window.prompt('Paste config JSON:') || ''
    if (!text) return
    let cfg: any
    try { cfg = JSON.parse(text) } catch { alert('Invalid JSON'); return }
    const err = validateConfig(cfg)
    if (err) { alert('Invalid config: ' + err); return }
    applyImportedConfig(cfg)
  }

  

  // Helper function to read string from memory using helper functions
  const readStringFromMemory = (mod: WASMModule, ptr: number): string => {
    let result = ''
    let currentPtr = ptr
    let charValue: number
    
    // Read characters until we hit null terminator (0)
    do {
      charValue = mod._read_uint8(currentPtr)
      if (charValue === 0) break
      result += String.fromCharCode(charValue)
      currentPtr++
    } while (charValue !== 0)
    
    return result
  }

  // Load animation definitions dynamically from WASM
  const loadAnimations = async (mod: WASMModule): Promise<AnimationInfo[]> => {
    const animCount = mod._anim_count()
    const animations: AnimationInfo[] = []
    
    // Allocate memory for string buffers
    const nameBufferSize = 64
    const nameBufferPtr = mod._malloc(nameBufferSize)
    
    // Allocate memory for parameter info (3 floats for min, max, default)
    const minMaxBufferPtr = mod._malloc(12) // 3 * 4 bytes for floats
    const typeBufferPtr = mod._malloc(1) // 1 byte for type
    
    try {
      for (let i = 0; i < animCount; i++) {
        // Get animation name
        mod._anim_name_safe(i, nameBufferPtr, nameBufferSize)
        
        // Read string from memory using helper function
        const name = readStringFromMemory(mod, nameBufferPtr)
        
        // Get parameters for this animation
        const paramCount = mod._anim_param_count(i)
        const parameters: ParameterInfo[] = []
        
        for (let j = 0; j < paramCount; j++) {
          const paramId = mod._anim_param_id(i, j)
          
          // Get parameter name
          mod._param_name_safe(paramId, nameBufferPtr, nameBufferSize)
          
          // Read parameter name from memory
          const paramName = readStringFromMemory(mod, nameBufferPtr)
          
          // Get parameter info - use separate buffer for numeric values
          mod._param_info(paramId, nameBufferPtr, minMaxBufferPtr, minMaxBufferPtr + 4, minMaxBufferPtr + 8, typeBufferPtr)
          
          // Read values from memory using helper functions (use float, not double)
          const min = mod._read_float(minMaxBufferPtr)
          const max = mod._read_float(minMaxBufferPtr + 4)
          const def = mod._read_float(minMaxBufferPtr + 8)
          const type = mod._read_int8(typeBufferPtr)
          
          // Map parameter type
          let paramType: 'bool' | 'range' | 'int' | 'enum' = 'range'
          switch (type) {
            case 0: paramType = 'bool'; break
            case 1: paramType = 'range'; break
            case 2: paramType = 'int'; break
            case 3: paramType = 'enum'; break
          }
          
          parameters.push({
            id: paramId,
            name: paramName,
            type: paramType,
            min,
            max,
            default: def
          })
        }
        
        animations.push({
          index: i,
          name,
          parameters
        })
      }
    } catch (error) {
      console.error('Error loading animations dynamically:', error)
      throw error
    } finally {
      // Free allocated memory
      mod._free(nameBufferPtr)
      mod._free(minMaxBufferPtr)
      mod._free(typeBufferPtr)
    }
    
    return animations
  }

  // Load WASM module
  useEffect(() => {
    const loadModule = async () => {
      try {
        // @ts-ignore
        const wasmModule = await import('../dist/sim.mjs')
        const mod = await wasmModule.default()
        setModule(mod)
        
        // Get constants from WASM
        const b = mod._anim_branches()
        const lpb = mod._anim_leds_per_branch()
        const total = mod._anim_total_leds()
        
        setBranches(b)
        setLedsPerBranch(lpb)
        setTotalLeds(total)
        
        // Initialize value arrays
        setLeftValues(new Float32Array(total))
        setRightValues(new Float32Array(total))
        
        // Load animation definitions
        const anims = await loadAnimations(mod)
        setAnimations(anims)
        
        // Initialize parameter states with defaults from anim_schema.h
        const defaultLeftParams: {[key: number]: number} = {}
        const defaultRightParams: {[key: number]: number} = {}
        
        // Set global speed parameter (not stored in per-tree states)
        mod._param_set(20, 1.0) // PID_GLOBAL_SPEED = 20
        
        // Initialize all parameters with their defaults from the loaded animations (excluding global speed)
        anims.forEach(anim => {
          anim.parameters.forEach(param => {
            if (param.id !== 20) { // Exclude global speed from per-tree states
              defaultLeftParams[param.id] = param.default
              defaultRightParams[param.id] = param.default
              mod._param_set(param.id, param.default)
            }
          })
        })
        
        setLeftParams(defaultLeftParams)
        setRightParams(defaultRightParams)
        
      } catch (error) {
        console.error('Failed to load WASM module:', error)
      }
    }
    
    loadModule()
  }, [])

  // Animation loop
  useEffect(() => {
    if (!module) return

    const animate = () => {
      if (!isPaused) {
        const currentTime = (Date.now() - startTimeRef.current) / 1000
        const t = currentTime

        // Apply global speed first
        module._param_set(20, globalControls.globalSpeed)

        // Apply left tree parameters and evaluate
        Object.entries(leftParams).forEach(([paramId, value]) => {
          module._param_set(parseInt(paramId), value)
        })
        module._anim_eval2_global(leftAnimation, t)
        const newLeftValues = new Float32Array(totalLeds)
        for (let i = 0; i < totalLeds; i++) {
          newLeftValues[i] = module._anim_get_value(i)
        }
        setLeftValues(newLeftValues)

        // Apply global speed again (in case it was overwritten)
        module._param_set(20, globalControls.globalSpeed)

        // Apply right tree parameters and evaluate
        Object.entries(rightParams).forEach(([paramId, value]) => {
          module._param_set(parseInt(paramId), value)
        })
        module._anim_eval2_global(rightAnimation, t)
        const newRightValues = new Float32Array(totalLeds)
        for (let i = 0; i < totalLeds; i++) {
          newRightValues[i] = module._anim_get_value(i)
        }
        setRightValues(newRightValues)
      }
      
      animationRef.current = requestAnimationFrame(animate)
    }

    animationRef.current = requestAnimationFrame(animate)
    
    return () => {
      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current)
      }
    }
  }, [module, isPaused, leftAnimation, rightAnimation, totalLeds, leftParams, rightParams, globalControls.globalSpeed])

  const handleLeftAnimationChange = (animationIndex: number) => {
    setLeftAnimation(animationIndex)
  }

  const handleRightAnimationChange = (animationIndex: number) => {
    setRightAnimation(animationIndex)
  }

  const handleParameterChange = (paramId: number, value: number, tree: 'left' | 'right') => {
    if (module) {
      module._param_set(paramId, value)
      if (paramId !== 20) { // Don't store global speed in per-tree states
        if (tree === 'left') {
          setLeftParams(prev => ({ ...prev, [paramId]: value }))
        } else {
          setRightParams(prev => ({ ...prev, [paramId]: value }))
        }
      }
    }
  }

  const getParameterValue = (paramId: number, tree: 'left' | 'right'): number => {
    const value = tree === 'left' ? leftParams[paramId] : rightParams[paramId]
    return value ?? 0
  }

  const handleGlobalChange = (key: keyof GlobalState, value: any) => {
    setGlobalControls(prev => ({ ...prev, [key]: value }))
    if (module && key === 'globalSpeed') {
      module._param_set(20, value) // PID_GLOBAL_SPEED = 20
    }
  }

  const togglePause = () => {
    setIsPaused(!isPaused)
    if (!isPaused) {
      startTimeRef.current = Date.now() - (startTimeRef.current ? Date.now() - startTimeRef.current : 0)
    }
  }

  return (
    <div className="app">
      <LEDVisualization 
        leftValues={leftValues}
        rightValues={rightValues}
        branches={branches}
        ledsPerBranch={ledsPerBranch}
      />
      
      <ControlPanel
        position="top-left"
        title="Left Tree"
        animations={animations}
        currentAnimation={leftAnimation}
        onAnimationChange={handleLeftAnimationChange}
        onParameterChange={(paramId, value) => handleParameterChange(paramId, value, 'left')}
        getParameterValue={(paramId) => getParameterValue(paramId, 'left')}
      />
      
      <ControlPanel
        position="top-right"
        title="Right Tree"
        animations={animations}
        currentAnimation={rightAnimation}
        onAnimationChange={handleRightAnimationChange}
        onParameterChange={(paramId, value) => handleParameterChange(paramId, value, 'right')}
        getParameterValue={(paramId) => getParameterValue(paramId, 'right')}
      />
      
      <ControlPanel
        position="bottom-left"
        title="Global"
        globalState={globalControls}
        onGlobalChange={handleGlobalChange}
        isPaused={isPaused}
        onPauseToggle={togglePause}
      />

      {/* Bottom-right import/export */}
      <div style={{position:'fixed', right:'16px', bottom:'16px', display:'flex', gap:'8px', zIndex:20}}>
        <button title="Import" aria-label="Import" onClick={doImport} style={{padding:'8px', borderRadius:'8px', background:'#1f2937', color:'#e5e7eb', border:'1px solid #374151'}}>
          <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="lucide lucide-import-icon lucide-import"><path d="M12 3v12"/><path d="m8 11 4 4 4-4"/><path d="M8 5H4a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V7a2 2 0 0 0-2-2h-4"/></svg>
        </button>
        <button title="Export" aria-label="Export" onClick={doExport} style={{padding:'8px', borderRadius:'8px', background:'#1f2937', color:'#e5e7eb', border:'1px solid #374151'}}>
          <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="lucide lucide-clipboard-copy-icon lucide-clipboard-copy"><rect width="8" height="4" x="8" y="2" rx="1" ry="1"/><path d="M8 4H6a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-2"/><path d="M16 4h2a2 2 0 0 1 2 2v4"/><path d="M21 14H11"/><path d="m15 10-4 4 4 4"/></svg>
        </button>
      </div>
    </div>
  )
}

export default App