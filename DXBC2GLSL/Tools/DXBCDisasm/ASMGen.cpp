/**************************************************************************
 *
 * Copyright 2013 Shenghua Lin, Minmin Gong
 * Copyright 2010 Luca Barbieri
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "ASMGen.hpp"
#include <iomanip>
#include <iostream>

// TODO: we should fix this to output the same syntax as fxc, if sm_dump_short_syntax is set

bool sm_dump_short_syntax = true;

ASMGen::ASMGen(boost::shared_ptr<ShaderProgram> const & program)
	: program_(program)
{
}

//disasm operands 
//��immediate32(l(a,b,c,d)) immediate64(d(a,b,c,d)) ��������r,v,o)��������ʽ
void ASMGen::Disasm(std::ostream& out, ShaderOperand const & op, ShaderImmType imm_type)
{
	if (op.neg)
	{
		out << '-';
	}
	if (op.abs)
	{
		out << '|';
	}
	if (SOT_IMMEDIATE32 == op.type)//l(a,b,c,d)
	{
		out << "l(";
		for (uint32_t i = 0; i < op.comps; ++ i)
		{
			if (i != 0)
			{
				out << ", ";
			}
			switch (imm_type)
			{
			case SIT_Float:
				out << op.imm_values[i].f32;
				break;

			case SIT_Int:
				out << op.imm_values[i].i32;
				break;

			case SIT_UInt:
				out << op.imm_values[i].u32;
				break;

			case SIT_Double:
				BOOST_ASSERT(false);
				break;
			}
		}
		out << ")";
	}
	else if (SOT_IMMEDIATE64 == op.type)//d(a,b,c,d)
	{
		out << "d(";
		for (uint32_t i = 0; i < op.comps; ++ i)
		{
			if (i != 0)
			{
				out << ", ";
			}
			switch (imm_type)
			{
			case SIT_Float:
			case SIT_Int:
			case SIT_UInt:
				BOOST_ASSERT(false);
				break;

			case SIT_Double:
				out << op.imm_values[i].f64;
				break;
			}
		}
		out << ")";
	}
	else//Ӧ��������v1 v2 o0 cb[1]�����ֱ�������ʽ
	{
		bool naked = false;
		//�ж�naked�Ƿ�Ϊture
		if (sm_dump_short_syntax)//���ֵ�Ѿ��ڱ��ļ���ͷ����Ϊtrue
		{
			switch (op.type)
			{
			case SOT_TEMP:
			case SOT_INPUT:
			case SOT_OUTPUT:
			case SOT_CONSTANT_BUFFER:
			case SOT_INDEXABLE_TEMP:
			case SOT_UNORDERED_ACCESS_VIEW:
			case SOT_THREAD_GROUP_SHARED_MEMORY:
			case SOT_RESOURCE:
			case SOT_SAMPLER:
				naked = true;
				break;

			default:
				naked = false;
				break;
			}
		}
		//�����������r��v��o��
		out << (sm_dump_short_syntax ? ShaderOperandTypeShortName(op.type) : ShaderOperandTypeName(op.type));

		if (op.indices[0].reg)
		{
			naked = false;//nakedΪtrue����indices[0]����û�б���ʽ����r0[���ֻ����ʽ],
			//nakedΪfalse����r[���ֻ����ʽ]������������û�г�����׺��
		}
		//����
		for (uint32_t i = 0; i < op.num_indices; ++ i)
		{
			if (!naked || (i != 0))//��һ����������Ҫ[]����cb0[22]0Ϊ��һ�㣬22�ǵڶ������[]
			{
				out << '[';
			}
			if (op.indices[i].reg)
			{
				this->Disasm(out, *op.indices[i].reg, imm_type);
				if (op.indices[i].disp)
				{
					out << '+' << op.indices[i].disp;
				}
			}
			else
			{
				out << op.indices[i].disp;
			}
			if (!naked || (i != 0))
			{
				out << ']';
			}
		}

		if (op.comps)
		{
			switch (op.mode)
			{
			case SOSM_MASK://xy��xyz��xyzw
				out << (sm_dump_short_syntax ? '.' : '!');
				for (uint32_t i = 0; i < op.comps; ++ i)
				{
					if (op.mask & (1 << i))
					{
						out << "xyzw"[i];
					}
				}

				break;

			case SOSM_SWIZZLE://xxyy��xxxx
				out << '.';
				for (uint32_t i = 0; i < op.comps; ++ i)
				{
					out << "xyzw"[op.swizzle[i]];
				}
				break;

			case SOSM_SCALAR:
				out << (sm_dump_short_syntax ? '.' : ':');
				out << "xyzw"[op.swizzle[0]];
				break;

			default:
				BOOST_ASSERT(false);
				break;
			}
		}
	}//end else
	if (op.abs)
	{
		out << '|';
	}
}

//disasm declarations
void ASMGen::Disasm(std::ostream& out, ShaderDecl const & dcl)
{
	out << ShaderOpcodeName(static_cast<ShaderOpcode>(dcl.opcode));
	switch (dcl.opcode)
	{
	case SO_DCL_GLOBAL_FLAGS:
		if (dcl.dcl_global_flags.allow_refactoring)
		{
			out << " refactoringAllowed";
		}
		if (dcl.dcl_global_flags.early_depth_stencil)
		{
			out << " forceEarlyDepthStencil";
		}
		if (dcl.dcl_global_flags.fp64)
		{
			out << " enableDoublePrecisionFloatOps";
		}
		if (dcl.dcl_global_flags.enable_raw_and_structured_in_non_cs)
		{
			out << " enableRawAndStructuredBuffers";
		}
		break;

	case SO_DCL_INPUT_PS:
	case SO_DCL_INPUT_PS_SIV:
	case SO_DCL_INPUT_PS_SGV:
		out << ' ' << ShaderInterpolationModeName(static_cast<ShaderInterpolationMode>(dcl.dcl_input_ps.interpolation));
		break;

	case SO_DCL_TEMPS:
		out << ' ' << dcl.num;
		break;

	case SO_DCL_INDEXABLE_TEMP:
		dcl.op->type = SOT_INDEXABLE_TEMP;
		break;

	case SO_DCL_RESOURCE:
		out << "_" << ShaderResourceDimensionName(static_cast<ShaderResourceDimension>(dcl.dcl_resource.target));
		if ((SRD_TEXTURE2DMS == dcl.dcl_resource.target) || (SRD_TEXTURE2DMSARRAY == dcl.dcl_resource.target))
		{
			if (dcl.dcl_resource.nr_samples)
			{
				out << " (" << dcl.dcl_resource.nr_samples << ")";
			}
			out << " (" << ShaderResourceReturnTypeName(static_cast<ShaderResourceReturnType>(dcl.rrt.x))
				<< "," << ShaderResourceReturnTypeName(static_cast<ShaderResourceReturnType>(dcl.rrt.y))
				<< "," << ShaderResourceReturnTypeName(static_cast<ShaderResourceReturnType>(dcl.rrt.z))
				<< "," << ShaderResourceReturnTypeName(static_cast<ShaderResourceReturnType>(dcl.rrt.w))
				<< ")";
		}
		break;

	case SO_DCL_UNORDERED_ACCESS_VIEW_TYPED:
		out << "_" << ShaderResourceDimensionName(static_cast<ShaderResourceDimension>(dcl.dcl_resource.target));
		out << " (" << ShaderResourceReturnTypeName(static_cast<ShaderResourceReturnType>(dcl.rrt.x))
			<< "," << ShaderResourceReturnTypeName(static_cast<ShaderResourceReturnType>(dcl.rrt.y))
			<< "," << ShaderResourceReturnTypeName(static_cast<ShaderResourceReturnType>(dcl.rrt.z))
			<< "," << ShaderResourceReturnTypeName(static_cast<ShaderResourceReturnType>(dcl.rrt.w))
			<< ")";
		break;

	case SO_IMMEDIATE_CONSTANT_BUFFER:
		{
			float const * data = reinterpret_cast<float const *>(&dcl.data[0]);
			out << "{\n";
			uint32_t vector_num = dcl.num / 4;
			for (uint32_t i = 0; i < vector_num; i++)
			{
				if (i != 0)out << ",\n";
				out << "{" << data[i * 4 + 0] << "," << data[i * 4 + 1] << "," << data[i * 4 + 2] << "," << data[i * 4 + 3] << "}";
			}
			out << "\n}\n";
		}
		break;

	default:
		break;
	}
	if (dcl.op)
	{
		out << ' ';
		this->Disasm(out, *dcl.op, GetImmType(dcl.opcode));
	}
	switch (dcl.opcode)
	{
	case SO_DCL_INDEX_RANGE:
		out << ' ' << dcl.num;
		break;

	case SO_DCL_INDEXABLE_TEMP:
		out << dcl.op->indices[0].disp << "[" << dcl.num << "]" << "," << dcl.num;
		break;

	case SO_DCL_CONSTANT_BUFFER:
		out << ", " << (dcl.dcl_constant_buffer.dynamic ? "dynamicIndexed" : "immediateIndexed");
		break;

	case SO_DCL_INPUT_SIV:
	case SO_DCL_INPUT_SGV:
	case SO_DCL_OUTPUT_SIV:
	case SO_DCL_OUTPUT_SGV:
	case SO_DCL_INPUT_PS_SIV:
	case SO_DCL_INPUT_PS_SGV:
		out << ", " << ShaderSystemValueName(static_cast<ShaderSystemValue>(dcl.num));
		break;

	case SO_DCL_SAMPLER:
		//out<<", "<<dcl.dcl_sampler.
		if (dcl.dcl_sampler.mono)
		{
			out << ", mode_mono";
		}
		else if (dcl.dcl_sampler.shadow)
		{
			out << ", mode_comparison";
		}
		else
		{
			out << ", mode_default";
		}
		break;

	default:
		break;
	}
}

//disasm instructions
void ASMGen::Disasm(std::ostream& out, ShaderInstruction const & insn)
{
	ShaderImmType sit = GetImmType(insn.opcode);

	out << ShaderOpcodeName(static_cast<ShaderOpcode>(insn.opcode));
	if (insn.insn.sat)
	{
		out << "_sat";
	}
	if ((SO_BREAKC == insn.opcode) || (SO_IF == insn.opcode) || (SO_CONTINUEC == insn.opcode)
		|| (SO_RETC == insn.opcode) || (SO_DISCARD == insn.opcode))
	{
		if (insn.insn.test_nz)
		{
			out << "_nz";
		}
		else
		{
			out << "_z";
		}
	}
	if (insn.extended)
	{
		out << " (" << static_cast<int>(insn.sample_offset[0]) << "," << static_cast<int>(insn.sample_offset[1])
			<< "," << static_cast<int>(insn.sample_offset[2]) << ")";
		out << " (" << ShaderResourceDimensionName(static_cast<ShaderResourceDimension>(insn.resource_target)) << ")";
		out << " (" << ShaderResourceReturnTypeName(static_cast<ShaderResourceReturnType>(insn.resource_return_type[0]))
			<< "," << ShaderResourceReturnTypeName(static_cast<ShaderResourceReturnType>(insn.resource_return_type[1]))
			<< "," << ShaderResourceReturnTypeName(static_cast<ShaderResourceReturnType>(insn.resource_return_type[2]))
			<< "," << ShaderResourceReturnTypeName(static_cast<ShaderResourceReturnType>(insn.resource_return_type[3]))
			<< ")";
	}
	if ((SO_RESINFO == insn.opcode) && insn.insn.resinfo_return_type)
	{
		out << "_" << ShaderResInfoReturnTypeName(static_cast<ShaderResInfoReturnType>(insn.insn.resinfo_return_type));
	}
	if (SO_SAMPLE_INFO == insn.opcode)
	{
		out << ShaderSampleInfoReturnTypeName(static_cast<ShaderSampleInfoReturnType>(insn.insn.resinfo_return_type));
	}

	for (uint32_t i = 0; i < insn.num_ops; ++ i)
	{
		if (i != 0)
		{
			out << ',';
		}
		out << ' ';
		this->Disasm(out, *insn.ops[i], sit);
	}
}

void ASMGen::ToASM(std::ostream& out)
{
	//diasm constant buffers
	this->Disasm(out, program_->cbuffers);
	//disasm resource bindings
	this->Disasm(out, program_->resource_bindings);
	//disasm input signature
	this->Disasm(out, program_->params_in, FOURCC_ISGN);
	//disasm output signature
	this->Disasm(out, program_->params_out, FOURCC_OSGN);
	out << "pvghdc"[program_->version.type] << "s_" << program_->version.major << "_" << program_->version.minor << "\n";
	for (size_t i = 0; i < program_->dcls.size(); ++ i)
	{
		this->Disasm(out, *program_->dcls[i]);
		out << "\n";
	}

	for (size_t i = 0; i < program_->insns.size(); ++ i)
	{
		this->Disasm(out, *program_->insns[i]);
		out << "\n";
	}
}

void ASMGen::Disasm(std::ostream& out, std::vector<DXBCSignatureParamDesc> const & signature, uint32_t fourcc)
{
	if (signature.empty())return;
	uint32_t count = signature.size();
	switch (fourcc)
	{
	case FOURCC_ISGN:
		out << "//\n//Input Signature:\n//\n";
		break;

	case FOURCC_OSGN:
		out << "//\n//Output Signature:\n//\n";
		break;

	default:
		BOOST_ASSERT(false);
		break;
	}
	out << "//" << std::setw(15) << "Name"
		<< std::setw(10) << "Index"
		<< std::setw(10) << "Register"
		<< std::setw(15) << "ValueType"
		<< std::setw(15) << "ComponentType"
		<< std::setw(10) << "Mask"
		<< "\n";
	out << "//" << std::setw(15) << "-------------"
		<< std::setw(10) << "--------"
		<< std::setw(10) << "--------"
		<< std::setw(15) << "-------------"
		<< std::setw(15) << "-------------"
		<< std::setw(10) << "-------"
		<< "\n";
	for (uint32_t i = 0; i < count; ++ i)
	{
		std::cout << "//"
			<< std::setw(15) << signature[i].semantic_name
			<< std::setw(10) << signature[i].semantic_index
			<< std::setw(10) << signature[i].register_index
			<< std::setw(15) << ShaderSystemValueName(static_cast<ShaderSystemValue>(signature[i].system_value_type))
			<< std::setw(15) << ShaderRegisterComponentTypeName(signature[i].component_type)
			<< std::setw(10) << uint32_t(signature[i].read_write_mask)
			<< "\n";
	}
	out << "//\n";
}

void ASMGen::Disasm(std::ostream& out, std::vector<DXBCConstantBuffer> const & cb)
{
	if (cb.empty())
	{
		return;
	}
	out << "//\n//Buffer Definitions:\n"
		<< "//\n";
	for (std::vector<DXBCConstantBuffer>::const_iterator cb_iter = cb.begin(); cb_iter != cb.end(); ++ cb_iter)
	{
		out << "//" << ShaderCBufferTypeName(cb_iter->desc.type) << " "
			<< cb_iter->desc.name << "\n"
			<< "//{\n";
		
		for (std::vector<DXBCShaderVariable>::const_iterator var_iter = cb_iter->vars.begin();
			var_iter != cb_iter->vars.end(); ++ var_iter)
		{
			BOOST_ASSERT_MSG(var_iter->has_type_desc, "CB member must have type desc");

			//array element count,0 if not a array
			uint32_t element_count = var_iter->type_desc.elements;
			out << "// ";
			switch (var_iter->type_desc.var_class)
			{
			case SVC_MATRIX_ROWS:
				out << "row_major "
					<< std::setw(5) << var_iter->type_desc.name
					<< var_iter->type_desc.rows << "x"
					<< var_iter->type_desc.columns;
				break;

			case SVC_MATRIX_COLUMNS:
				out << std::setw(15) << var_iter->type_desc.name
					<< var_iter->type_desc.rows << "x"
					<< var_iter->type_desc.columns;
				break;

			case SVC_VECTOR:
				out << std::setw(17) << var_iter->type_desc.name
					<< var_iter->type_desc.columns;
				break;

			case SVC_SCALAR:
				out << std::setw(18) << var_iter->type_desc.name;
				break;

				// TODO: to be fixed here
			case SVC_STRUCT:
				out << ShaderVariableClassName(var_iter->type_desc.var_class)
					<< " " << var_iter->type_desc.offset;
				break;

			default:
				BOOST_ASSERT_MSG(false, "Unhandled type");
				break;
			}
			out << std::setw(20) << var_iter->var_desc.name;
			if (element_count)
			{
				out << "[" << element_count << "]";
			}
			out << ";"
				<< " //" << "Offset: " << std::setw(5)
				<< var_iter->var_desc.start_offset
				<< " Size: " << std::setw(5)
				<< var_iter->var_desc.size;
			if (!var_iter->var_desc.flags)
			{
				out << " [unused]";
			}
			out << "\n";
		}
		out << "//}\n";
	}
}

void ASMGen::Disasm(std::ostream& out, std::vector<DXBCInputBindDesc> const & bindings)
{
	if (bindings.empty())
	{
		return;
	}

	out << "//\n//Resource Definitions:\n"
		<< "//\n";
	out << "//" << std::setw(20) << "Name"
		<< std::setw(30) << "Type"
		<< std::setw(10) << "Slot\n";
	out << "//" << std::setw(20) << "--------------"
		<< std::setw(30) << "--------------------------"
		<< std::setw(10) << "--------\n";
	
	for (std::vector<DXBCInputBindDesc>::const_iterator iter = bindings.begin();
		iter != bindings.end(); ++ iter)
	{
		out << "//";
		out << std::setw(20) << iter->name
			<< std::setw(30) << ShaderInputTypeName(iter->type)
			<< std::setw(10) << iter->bind_point
			<< "\n";
	}
}