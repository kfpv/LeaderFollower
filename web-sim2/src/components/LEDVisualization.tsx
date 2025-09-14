import React, { useRef, useEffect } from 'react'

interface LEDVisualizationProps {
  leftValues: Float32Array
  rightValues: Float32Array
  branches: number
  ledsPerBranch: number
}

const LEDVisualization: React.FC<LEDVisualizationProps> = ({ 
  leftValues, 
  rightValues, 
  branches, 
  ledsPerBranch 
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null)

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return

    const ctx = canvas.getContext('2d')
    if (!ctx) return

    // Set canvas size to window size
    canvas.width = window.innerWidth
    canvas.height = window.innerHeight

    // Clear canvas
    ctx.fillStyle = '#000'
    ctx.fillRect(0, 0, canvas.width, canvas.height)

    const centerX = canvas.width / 2
    const centerY = canvas.height / 2
    const radius = Math.min(canvas.width, canvas.height) * 0.3

    // Draw left tree
    drawSnowflake(ctx, centerX - radius - 50, centerY, radius, leftValues, branches, ledsPerBranch, '#8b5cf6')
    
    // Draw right tree
    drawSnowflake(ctx, centerX + radius + 50, centerY, radius, rightValues, branches, ledsPerBranch, '#06b6d4')

  }, [leftValues, rightValues, branches, ledsPerBranch])

  const drawSnowflake = (
    ctx: CanvasRenderingContext2D, 
    centerX: number, 
    centerY: number, 
    radius: number, 
    values: Float32Array, 
    branches: number, 
    ledsPerBranch: number,
    color: string
  ) => {
    const angleStep = (2 * Math.PI) / branches

    for (let branch = 0; branch < branches; branch++) {
      const angle = branch * angleStep - Math.PI / 2
      const endX = centerX + Math.cos(angle) * radius
      const endY = centerY + Math.sin(angle) * radius

      // Draw branch line
      ctx.strokeStyle = '#333'
      ctx.lineWidth = 2
      ctx.beginPath()
      ctx.moveTo(centerX, centerY)
      ctx.lineTo(endX, endY)
      ctx.stroke()

      // Draw LEDs along the branch
      for (let led = 0; led < ledsPerBranch; led++) {
        const t = (led + 1) / ledsPerBranch
        const ledX = centerX + (endX - centerX) * t
        const ledY = centerY + (endY - centerY) * t

        const valueIndex = branch * ledsPerBranch + led
        const brightness = values[valueIndex] || 0

        // LED glow effect
        const gradient = ctx.createRadialGradient(ledX, ledY, 0, ledX, ledY, 8)
        gradient.addColorStop(0, `${color}${Math.floor(brightness * 255).toString(16).padStart(2, '0')}`)
        gradient.addColorStop(1, `${color}00`)

        ctx.fillStyle = gradient
        ctx.beginPath()
        ctx.arc(ledX, ledY, 8, 0, 2 * Math.PI)
        ctx.fill()

        // LED core
        ctx.fillStyle = color
        ctx.globalAlpha = brightness
        ctx.beginPath()
        ctx.arc(ledX, ledY, 4, 0, 2 * Math.PI)
        ctx.fill()
        ctx.globalAlpha = 1
      }
    }

    // Draw center hub
    ctx.fillStyle = '#666'
    ctx.beginPath()
    ctx.arc(centerX, centerY, 8, 0, 2 * Math.PI)
    ctx.fill()
  }

  return (
    <canvas
      ref={canvasRef}
      className="absolute top-0 left-0 w-full h-full"
    />
  )
}

export default LEDVisualization