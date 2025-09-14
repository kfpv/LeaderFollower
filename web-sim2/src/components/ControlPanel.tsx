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

interface ControlPanelProps {
  position: 'top-left' | 'top-right' | 'bottom-left'
  title: string
  animations?: AnimationInfo[]
  currentAnimation?: number
  onAnimationChange?: (animationIndex: number) => void
  onParameterChange?: (paramId: number, value: number) => void
  getParameterValue?: (paramId: number) => number
  globalState?: GlobalState
  onGlobalChange?: (key: keyof GlobalState, value: any) => void
  isPaused?: boolean
  onPauseToggle?: () => void
}

const ControlPanel = ({ 
  position, 
  title, 
  animations, 
  currentAnimation, 
  onAnimationChange, 
  onParameterChange,
  getParameterValue,
  globalState, 
  onGlobalChange, 
  isPaused, 
  onPauseToggle 
}: ControlPanelProps) => {
  const positionClasses = {
    'top-left': 'top-12 left-12',
    'top-right': 'top-12 right-12', 
    'bottom-left': 'bottom-12 left-12'
  }

  const isGlobal = title === 'Global'
  const isTreeControl = animations !== undefined

  const getCurrentAnimation = () => {
    if (!animations || currentAnimation === undefined) return null
    return animations.find(anim => anim.index === currentAnimation)
  }

  const renderParameterControl = (param: ParameterInfo) => {
    if (!onParameterChange || !getParameterValue) return null

    const currentValue = getParameterValue(param.id)
    const handleChange = (value: number) => {
      onParameterChange(param.id, value)
    }

    switch (param.type) {
      case 'bool':
        return (
          <label key={param.id} className="flex items-center gap-2">
            {param.name}
            <input 
              type="checkbox" 
              checked={currentValue !== 0}
              onChange={(e) => handleChange(e.target.checked ? 1 : 0)}
              className="w-4 h-4"
            />
          </label>
        )
      
      case 'range':
        return (
          <label key={param.id} className="flex flex-col gap-2">
            <div className="flex items-center gap-2">
              {param.name}
              <input 
                type="range" 
                min={param.min} 
                max={param.max} 
                step="0.01"
                value={currentValue}
                onChange={(e) => handleChange(parseFloat(e.target.value))}
                className="flex-1 [&::-webkit-slider-runnable-track]:bg-gray-600 [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:h-4 [&::-webkit-slider-thumb]:w-4 [&::-webkit-slider-thumb]:rounded-full [&::-webkit-slider-thumb]:bg-white"
              />
              <input 
                type="number" 
                min={param.min} 
                max={param.max} 
                step="0.01"
                value={currentValue}
                onChange={(e) => handleChange(parseFloat(e.target.value) || 0)}
                className="w-16 px-1 py-0.5 text-sm bg-gray-800 border border-gray-600 rounded"
              />
            </div>
          </label>
        )
      
      case 'int':
        return (
          <label key={param.id} className="flex flex-col gap-2">
            <div className="flex items-center gap-2">
              {param.name}
              <input 
                type="range" 
                min={param.min} 
                max={param.max} 
                step="1"
                value={currentValue}
                onChange={(e) => handleChange(parseInt(e.target.value))}
                className="flex-1"
              />
              <input 
                type="number" 
                min={param.min} 
                max={param.max} 
                step="1"
                value={Math.round(currentValue)}
                onChange={(e) => handleChange(parseInt(e.target.value) || 0)}
                className="w-12 px-1 py-0.5 text-sm bg-gray-800 border border-gray-600 rounded"
              />
            </div>
          </label>
        )
      
      default:
        return null
    }
  }

  const renderTreeControls = () => {
    if (!animations || currentAnimation === undefined || !onAnimationChange) return null

    const currentAnim = getCurrentAnimation()

    return (
      <>
        <label className="flex items-center gap-2">
          Animation
          <select 
            value={currentAnimation}
            onChange={(e) => onAnimationChange(parseInt(e.target.value))}
            className="bg-gray-800 text-white px-2 py-1 rounded border border-gray-600"
          >
            {animations.map(anim => (
              <option key={anim.index} value={anim.index}>
                {anim.name}
              </option>
            ))}
          </select>
        </label>
        
        {currentAnim && currentAnim.parameters.map(renderParameterControl)}
      </>
    )
  }

  const renderGlobalControls = () => (
    <>
      <label className="flex flex-col gap-2">
        <div className="flex items-center gap-2">
          Speed
          <input 
            type="range" 
            min="0" 
            max="20" 
            step="0.01"
            value={globalState!.speed}
            onChange={(e) => onGlobalChange!('speed', parseFloat(e.target.value))}
            className="flex-1 [&::-webkit-slider-runnable-track]:bg-gray-600 [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:h-4 [&::-webkit-slider-thumb]:w-4 [&::-webkit-slider-thumb]:rounded-full [&::-webkit-slider-thumb]:bg-white"
          />
          <input 
            type="number" 
            min="0" 
            max="20" 
            step="0.01"
            value={globalState!.speed}
            onChange={(e) => onGlobalChange!('speed', parseFloat(e.target.value) || 0)}
            className="w-16 px-1 py-0.5 text-sm bg-gray-800 border border-gray-600 rounded"
          />
        </div>
      </label>
      
      <button 
        onClick={onPauseToggle}
        className={`px-4 py-2 rounded font-medium ${
          isPaused 
            ? 'bg-green-600 hover:bg-green-700' 
            : 'bg-red-600 hover:bg-red-700'
        } text-white transition-colors`}
      >
        {isPaused ? 'Resume' : 'Pause'}
      </button>
    </>
  )

  return (
    <details 
      open 
      className={`fixed z-10 min-w-[220px] text-gray-200 bg-gray-900 border border-gray-700 rounded-lg shadow-lg ${positionClasses[position]}`}
    >
      <summary className="cursor-pointer list-none px-3 py-2 font-semibold hover:bg-gray-800 rounded-t-lg">
        {title}
      </summary>
      <div className="flex flex-col gap-3 p-3 pt-0">
        {isGlobal ? renderGlobalControls() : isTreeControl ? renderTreeControls() : null}
      </div>
    </details>
  )
}

export default ControlPanel