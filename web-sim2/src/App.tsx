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
  UTF8ToString: (ptr: number) => string
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
}



interface GlobalState {
  speed: number
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
    speed: 6
  })
  
  // Store separate parameter values for left and right trees
  const [leftParams, setLeftParams] = useState<{[key: number]: number}>({
    2: 0, // phase
    4: 1, // branch
    5: 0, // invert
    3: 3, // width
    6: 0.5, // level
    7: 0 // singleIndex
  })
  
  const [rightParams, setRightParams] = useState<{[key: number]: number}>({
    2: 0, // phase
    4: 1, // branch
    5: 0, // invert
    3: 3, // width
    6: 0.5, // level
    7: 0 // singleIndex
  })
  
  const animationRef = useRef<number | null>(null)
  const startTimeRef = useRef<number>(Date.now())

  

  // Load animation definitions using static names from ANIMS array
  const loadAnimations = async (): Promise<AnimationInfo[]> => {
    // Static animation names from ANIMS array in implementations.cpp
    const staticAnimations = [
      { index: 0, name: "Static" },
      { index: 1, name: "Wave" },
      { index: 2, name: "Pulse" },
      { index: 3, name: "Chase" },
      { index: 4, name: "Single" }
    ]
    
    // Parameter definitions based on anim_schema.h
    const parameterDefs: { [key: number]: ParameterInfo } = {
      2: { id: 2, name: 'phase', type: 'range', min: -6.283, max: 6.283, default: 0 },
      3: { id: 3, name: 'width', type: 'int', min: 1, max: 8, default: 3 },
      4: { id: 4, name: 'branch', type: 'bool', min: 0, max: 1, default: 0 },
      5: { id: 5, name: 'invert', type: 'bool', min: 0, max: 1, default: 0 },
      6: { id: 6, name: 'level', type: 'range', min: 0, max: 1, default: 0.5 },
      7: { id: 7, name: 'singleIndex', type: 'int', min: 0, max: 63, default: 0 }
    }
    
    // Animation parameter mappings (speed is global, not included here)
    const animationParams: { [key: number]: number[] } = {
      0: [6],     // Static: level
      1: [2, 4, 5],    // Wave: phase, branch, invert (speed is global)
      2: [2, 4],       // Pulse: phase, branch (speed is global)
      3: [3, 4],       // Chase: width, branch (speed is global)
      4: [7]           // Single: singleIndex (speed is global)
    }
    
    const animations: AnimationInfo[] = []
    
    for (const staticAnim of staticAnimations) {
      const paramIds = animationParams[staticAnim.index] || []
      const parameters: ParameterInfo[] = []
      
      for (const paramId of paramIds) {
        const paramInfo = parameterDefs[paramId]
        if (paramInfo) {
          parameters.push(paramInfo)
        }
      }
      
      animations.push({
        index: staticAnim.index,
        name: staticAnim.name,
        parameters
      })
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
        const anims = await loadAnimations()
        setAnimations(anims)
        
        // Set default parameters for initial animations
        mod._param_set(1, 3.0) // global speed
        mod._param_set(2, 0.0) // phase for wave animation
        mod._param_set(4, 1.0) // branch for wave animation
        mod._param_set(5, 0.0) // invert for wave animation
        
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

        // Set parameters for left tree and evaluate
        Object.entries(leftParams).forEach(([paramId, value]) => {
          module._param_set(parseInt(paramId), value)
        })
        module._anim_eval2_global(leftAnimation, t)
        const newLeftValues = new Float32Array(totalLeds)
        for (let i = 0; i < totalLeds; i++) {
          newLeftValues[i] = module._anim_get_value(i)
        }
        setLeftValues(newLeftValues)

        // Set parameters for right tree and evaluate
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
  }, [module, isPaused, leftAnimation, rightAnimation, totalLeds, leftParams, rightParams])

  const handleLeftAnimationChange = (animationIndex: number) => {
    setLeftAnimation(animationIndex)
  }

  const handleRightAnimationChange = (animationIndex: number) => {
    setRightAnimation(animationIndex)
  }

  const handleParameterChange = (paramId: number, value: number, tree: 'left' | 'right') => {
    if (module) {
      module._param_set(paramId, value)
      if (tree === 'left') {
        setLeftParams(prev => ({ ...prev, [paramId]: value }))
      } else {
        setRightParams(prev => ({ ...prev, [paramId]: value }))
      }
    }
  }

  const getParameterValue = (paramId: number, tree: 'left' | 'right'): number => {
    if (tree === 'left') {
      return leftParams[paramId] || 0
    } else {
      return rightParams[paramId] || 0
    }
  }

  const handleGlobalChange = (key: keyof GlobalState, value: any) => {
    setGlobalControls(prev => ({ ...prev, [key]: value }))
    if (module && key === 'speed') {
      module._param_set(1, value)
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
    </div>
  )
}

export default App